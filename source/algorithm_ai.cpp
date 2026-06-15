#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <string>
#include <stdlib.h>

#include "core.h"

ConVar ebot_ai_chat("ebot_ai_chat", "1");
ConVar ebot_ai_chat_autostart("ebot_ai_chat_autostart", "1");
ConVar ebot_ai_chat_dir("ebot_ai_chat_dir", "");
ConVar ebot_ai_chat_python("ebot_ai_chat_python", "python");
ConVar ebot_ai_chat_host("ebot_ai_chat_host", "127.0.0.1");
ConVar ebot_ai_chat_port("ebot_ai_chat_port", "7867");
ConVar ebot_ai_chat_path("ebot_ai_chat_path", "/chat");
ConVar ebot_ai_chat_timeout_ms("ebot_ai_chat_timeout_ms", "700");
ConVar ebot_ai_chat_cooldown("ebot_ai_chat_cooldown", "4.0");

namespace
{
	struct AIState
	{
		tthread::mutex lock;
		bool busy{false};
		bool hasResponse{false};
		float nextPromptTime{0.0f};
		char response[192]{};
	};

	struct AIRequest
	{
		char player[64]{};
		char text[256]{};
	};

	AIState g_aiState;

#ifdef _WIN32
	PROCESS_INFORMATION g_helperProcess{};
	bool g_helperOwned{false};
#endif

	static void CopyClean(char* dst, const int dstSize, const char* src, const bool chatSafe)
	{
		if (!dst || dstSize <= 0)
			return;

		dst[0] = '\0';
		if (IsNullString(src))
			return;

		int out = 0;
		for (int i = 0; src[i] && out < dstSize - 1; i++)
		{
			char ch = src[i];
			if (ch == '\r' || ch == '\n' || ch == '\t')
				ch = ' ';

			if (chatSafe && (ch == ';' || ch == '"' || ch == '\\'))
				ch = '\'';

			dst[out++] = ch;
		}
		dst[out] = '\0';

		while (out > 0 && dst[out - 1] == ' ')
			dst[--out] = '\0';

		if (dst[0] == '"')
		{
			for (int i = 0; dst[i]; i++)
				dst[i] = dst[i + 1];
		}

		out = cstrlen(dst);
		if (out > 0 && dst[out - 1] == '"')
			dst[out - 1] = '\0';
	}

	static bool LooksLikePrompt(const char* text)
	{
		if (IsNullString(text))
			return false;

		char* mutableText = const_cast<char*>(text);
		if (!cstrncmp(mutableText, "/chat", 5) || !cstrncmp(mutableText, "!chat", 5) ||
			!cstrncmp(mutableText, "/ai", 3) || !cstrncmp(mutableText, "!ai", 3) ||
			cstrstr(mutableText, "?") || cstrstr(mutableText, "bot") || cstrstr(mutableText, "Bot") || cstrstr(mutableText, "BOT") ||
			cstrstr(mutableText, "ebot") || cstrstr(mutableText, "Ebot") || cstrstr(mutableText, "ai") || cstrstr(mutableText, "AI"))
			return true;

		return false;
	}

	static const char* SkipChatPrefix(const char* text)
	{
		if (IsNullString(text))
			return text;

		if (!cstrncmp(text, "/chat", 5) || !cstrncmp(text, "!chat", 5))
			text += 5;
		else if (!cstrncmp(text, "/ai", 3) || !cstrncmp(text, "!ai", 3))
			text += 3;

		while (*text == ' ')
			text++;

		return text;
	}

	static std::string JsonEscape(const char* input)
	{
		std::string out;
		if (!input)
			return out;

		for (int i = 0; input[i]; i++)
		{
			const unsigned char ch = static_cast<unsigned char>(input[i]);
			if (ch == '"' || ch == '\\')
			{
				out += '\\';
				out += static_cast<char>(ch);
			}
			else if (ch == '\r' || ch == '\n' || ch == '\t')
				out += ' ';
			else if (ch >= 32)
				out += static_cast<char>(ch);
		}
		return out;
	}

	static bool ExtractReply(const std::string& body, char* output, const int outputSize)
	{
		const std::string key = "\"reply\"";
		size_t pos = body.find(key);
		if (pos == std::string::npos)
			return false;

		pos = body.find(':', pos + key.length());
		if (pos == std::string::npos)
			return false;

		pos = body.find('"', pos + 1);
		if (pos == std::string::npos)
			return false;

		std::string reply;
		bool escaped = false;
		for (size_t i = pos + 1; i < body.length(); i++)
		{
			const char ch = body[i];
			if (escaped)
			{
				reply += ch == 'n' || ch == 'r' || ch == 't' ? ' ' : ch;
				escaped = false;
				continue;
			}

			if (ch == '\\')
			{
				escaped = true;
				continue;
			}

			if (ch == '"')
				break;

			reply += ch;
		}

		CopyClean(output, outputSize, reply.c_str(), true);
		return !IsNullString(output);
	}

	static bool HttpPost(const AIRequest* request, char* output, const int outputSize)
	{
#ifndef _WIN32
		(void)request;
		(void)output;
		(void)outputSize;
		return false;
#else
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			return false;

		const char* host = ebot_ai_chat_host.GetString();
		const char* path = ebot_ai_chat_path.GetString();
		const int port = ebot_ai_chat_port.GetInt() > 0 ? ebot_ai_chat_port.GetInt() : 7867;
		const int timeout = ebot_ai_chat_timeout_ms.GetInt() > 0 ? ebot_ai_chat_timeout_ms.GetInt() : 700;
		if (IsNullString(host))
			host = "127.0.0.1";
		if (IsNullString(path))
			path = "/chat";

		addrinfo hints{};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		addrinfo* result = nullptr;
		char portText[16];
		snprintf(portText, sizeof(portText), "%d", port);

		if (getaddrinfo(host, portText, &hints, &result) != 0 || !result)
		{
			WSACleanup();
			return false;
		}

		SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (sock == INVALID_SOCKET)
		{
			freeaddrinfo(result);
			WSACleanup();
			return false;
		}

		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

		if (connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR)
		{
			closesocket(sock);
			freeaddrinfo(result);
			WSACleanup();
			return false;
		}
		freeaddrinfo(result);

		const std::string body = std::string("{\"player\":\"") + JsonEscape(request->player) + "\",\"message\":\"" + JsonEscape(request->text) + "\"}";
		const std::string http = std::string("POST ") + path + " HTTP/1.1\r\nHost: " + host + ":" + portText +
			"\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + std::to_string(body.length()) + "\r\n\r\n" + body;

		if (send(sock, http.c_str(), static_cast<int>(http.length()), 0) == SOCKET_ERROR)
		{
			closesocket(sock);
			WSACleanup();
			return false;
		}

		std::string response;
		char buffer[512];
		for (;;)
		{
			const int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
			if (received <= 0)
				break;

			buffer[received] = '\0';
			response += buffer;
			if (response.length() > 8192)
				break;
		}

		closesocket(sock);
		WSACleanup();

		const size_t bodyStart = response.find("\r\n\r\n");
		const std::string json = bodyStart == std::string::npos ? response : response.substr(bodyStart + 4);
		return ExtractReply(json, output, outputSize);
#endif
	}

#ifdef _WIN32
	static bool CanConnectToHelper()
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			return false;

		const char* host = ebot_ai_chat_host.GetString();
		const int port = ebot_ai_chat_port.GetInt() > 0 ? ebot_ai_chat_port.GetInt() : 7867;
		if (IsNullString(host))
			host = "127.0.0.1";

		addrinfo hints{};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		addrinfo* result = nullptr;
		char portText[16];
		snprintf(portText, sizeof(portText), "%d", port);

		if (getaddrinfo(host, portText, &hints, &result) != 0 || !result)
		{
			WSACleanup();
			return false;
		}

		SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (sock == INVALID_SOCKET)
		{
			freeaddrinfo(result);
			WSACleanup();
			return false;
		}

		const int timeout = 250;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
		const bool connected = connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) != SOCKET_ERROR;

		closesocket(sock);
		freeaddrinfo(result);
		WSACleanup();
		return connected;
	}

	static void NormalizeSlashes(char* text)
	{
		if (!text)
			return;

		for (int i = 0; text[i]; i++)
		{
			if (text[i] == '/')
				text[i] = '\\';
		}
	}

	static bool DirectoryExists(const char* path)
	{
		const DWORD attrs = GetFileAttributesA(path);
		return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
	}

	static bool FileExists(const char* path)
	{
		const DWORD attrs = GetFileAttributesA(path);
		return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
	}

	static bool GetDefaultHelperDir(char* output, const int outputSize)
	{
		if (!output || outputSize <= 0)
			return false;

		output[0] = '\0';
		const char* configured = ebot_ai_chat_dir.GetString();
		if (!IsNullString(configured))
		{
			snprintf(output, outputSize, "%s", configured);
			NormalizeSlashes(output);
			return DirectoryExists(output);
		}

		HMODULE module = nullptr;
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCSTR>(&GetDefaultHelperDir), &module))
			return false;

		char modulePath[MAX_PATH];
		if (!GetModuleFileNameA(module, modulePath, sizeof(modulePath)))
			return false;

		NormalizeSlashes(modulePath);
		char* slash = strrchr(modulePath, '\\');
		if (!slash)
			return false;

		*slash = '\0'; // ...\addons\ebot\dlls
		slash = strrchr(modulePath, '\\');
		if (slash && !cstricmp(slash + 1, "dlls"))
			*slash = '\0'; // ...\addons\ebot

		snprintf(output, outputSize, "%s\\algorithm_ai", modulePath);
		return DirectoryExists(output);
	}

	static bool HelperProcessStillAlive()
	{
		if (!g_helperOwned || !g_helperProcess.hProcess)
			return false;

		DWORD exitCode = 0;
		return GetExitCodeProcess(g_helperProcess.hProcess, &exitCode) && exitCode == STILL_ACTIVE;
	}
#endif

	static void Worker(void* data)
	{
		AIRequest* request = static_cast<AIRequest*>(data);
		char reply[192]{};

		if (!HttpPost(request, reply, sizeof(reply)))
			CopyClean(reply, sizeof(reply), "AI helper is not ready yet.", true);

		{
			tthread::lock_guard<tthread::mutex> guard(g_aiState.lock);
			CopyClean(g_aiState.response, sizeof(g_aiState.response), reply, true);
			g_aiState.hasResponse = !IsNullString(g_aiState.response);
			g_aiState.busy = false;
		}

		delete request;
	}

	static edict_t* PickSpeakerBot()
	{
		for (Bot* const& bot : g_botManager->m_bots)
		{
			if (!bot || !bot->m_isAlive)
				continue;

			edict_t* ent = bot->GetEntity();
			if (IsValidBot(ent))
				return ent;
		}

		return nullptr;
	}
}

void AlgorithmAI_StartHelper(void)
{
#ifdef _WIN32
	if (!ebot_ai_chat.GetBool() || !ebot_ai_chat_autostart.GetBool())
		return;

	if (HelperProcessStillAlive() || CanConnectToHelper())
		return;

	char helperDir[MAX_PATH];
	if (!GetDefaultHelperDir(helperDir, sizeof(helperDir)))
		return;

	char scriptPath[MAX_PATH];
	snprintf(scriptPath, sizeof(scriptPath), "%s\\algorithm_ai_server.py", helperDir);
	if (!FileExists(scriptPath))
		return;

	const char* python = ebot_ai_chat_python.GetString();
	if (IsNullString(python))
		python = "python";

	SetEnvironmentVariableA("ALGORITHM_AI_THREADS", "2");
	SetEnvironmentVariableA("ALGORITHM_AI_CTX", "512");
	SetEnvironmentVariableA("ALGORITHM_AI_MAX_TOKENS", "40");
	SetEnvironmentVariableA("ALGORITHM_AI_MODEL_URL", "https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/qwen2.5-1.5b-instruct-q3_k_m.gguf");
	SetEnvironmentVariableA("ALGORITHM_AI_TEMPLATE", "qwen");

	char command[1024];
	snprintf(command, sizeof(command), "\"%s\" \"%s\"", python, scriptPath);

	STARTUPINFOA si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi{};
	if (!CreateProcessA(nullptr, command, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, helperDir, &si, &pi))
		return;

	g_helperProcess = pi;
	g_helperOwned = true;
#endif
}

void AlgorithmAI_StopHelper(void)
{
#ifdef _WIN32
	if (!g_helperOwned)
		return;

	if (g_helperProcess.hProcess)
	{
		DWORD exitCode = 0;
		if (GetExitCodeProcess(g_helperProcess.hProcess, &exitCode) && exitCode == STILL_ACTIVE)
		{
			TerminateProcess(g_helperProcess.hProcess, 0);
			WaitForSingleObject(g_helperProcess.hProcess, 3000);
		}

		CloseHandle(g_helperProcess.hProcess);
	}

	if (g_helperProcess.hThread)
		CloseHandle(g_helperProcess.hThread);

	cmemset(&g_helperProcess, 0, sizeof(g_helperProcess));
	g_helperOwned = false;
#endif
}

void AlgorithmAI_OnPlayerChat(edict_t* player, const char* text)
{
	if (!ebot_ai_chat.GetBool() || FNullEnt(player) || IsValidBot(player))
		return;

	char cleanText[256];
	CopyClean(cleanText, sizeof(cleanText), text, false);
	if (!LooksLikePrompt(cleanText))
		return;

	AlgorithmAI_StartHelper();

	const float now = engine->GetTime();
	{
		tthread::lock_guard<tthread::mutex> guard(g_aiState.lock);
		if (g_aiState.busy || g_aiState.nextPromptTime > now)
			return;

		g_aiState.busy = true;
		g_aiState.nextPromptTime = now + cmax(1.0f, ebot_ai_chat_cooldown.GetFloat());
	}

	AIRequest* request = new(std::nothrow) AIRequest;
	if (!request)
	{
		tthread::lock_guard<tthread::mutex> guard(g_aiState.lock);
		g_aiState.busy = false;
		return;
	}

	CopyClean(request->player, sizeof(request->player), GetEntityName(player), true);
	CopyClean(request->text, sizeof(request->text), SkipChatPrefix(cleanText), true);

	tthread::thread* thread = new(std::nothrow) tthread::thread(Worker, request);
	if (!thread)
	{
		delete request;
		tthread::lock_guard<tthread::mutex> guard(g_aiState.lock);
		g_aiState.busy = false;
		return;
	}

	thread->detach();
	delete thread;
}

void AlgorithmAI_Think(void)
{
	if (!ebot_ai_chat.GetBool())
		return;

	char reply[192]{};
	{
		tthread::lock_guard<tthread::mutex> guard(g_aiState.lock);
		if (!g_aiState.hasResponse)
			return;

		CopyClean(reply, sizeof(reply), g_aiState.response, true);
		g_aiState.response[0] = '\0';
		g_aiState.hasResponse = false;
	}

	edict_t* speaker = PickSpeakerBot();
	if (speaker && !IsNullString(reply))
		FakeClientCommand(speaker, "say \"%s\"", reply);
}
