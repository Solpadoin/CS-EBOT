#include <amxmodx>
#include <reapi_vtc>

#define PLUGIN  "EBOT TTS VTC Bridge"
#define VERSION "0.1"
#define AUTHOR  "Solpadoin + Codex"

new g_cvarEnabled;
new g_cvarQueueFile;
new g_cvarDebug;
new g_maxClients;

public plugin_init()
{
	register_plugin(PLUGIN, VERSION, AUTHOR);

	g_cvarEnabled = register_cvar("ebot_tts_enabled", "1");
	g_cvarQueueFile = register_cvar("ebot_tts_queue_file", "addons/amxmodx/data/ebot_tts_queue.ini");
	g_cvarDebug = register_cvar("ebot_tts_debug", "1");

	register_srvcmd("ebot_tts_play", "Command_PlayTTS");

	g_maxClients = get_maxplayers();
	set_task(0.35, "Task_FlushQueue", _, _, _, "b");
}

public Command_PlayTTS()
{
	new soundFile[192];
	read_argv(1, soundFile, charsmax(soundFile));
	trim(soundFile);

	if (!soundFile[0])
	{
		server_print("[ebot_tts] Usage: ebot_tts_play sound/ebot_tts/file.wav");
		return PLUGIN_HANDLED;
	}

	PlayVoiceSound(soundFile);
	return PLUGIN_HANDLED;
}

public Task_FlushQueue()
{
	if (!get_pcvar_num(g_cvarEnabled))
		return;

	new queueFile[192];
	get_pcvar_string(g_cvarQueueFile, queueFile, charsmax(queueFile));

	if (!file_exists(queueFile))
		return;

	new fp = fopen(queueFile, "rt");
	if (!fp)
		return;

	new sounds[8][192];
	new count = 0;

	while (!feof(fp) && count < sizeof(sounds))
	{
		fgets(fp, sounds[count], charsmax(sounds[]));
		trim(sounds[count]);

		if (!sounds[count][0] || sounds[count][0] == ';' || sounds[count][0] == '#')
			continue;

		count++;
	}

	fclose(fp);
	delete_file(queueFile);

	for (new i = 0; i < count; i++)
		PlayVoiceSound(sounds[i]);
}

PlayVoiceSound(const soundFile[])
{
	if (get_pcvar_num(g_cvarDebug))
		server_print("[ebot_tts] VTC_PlaySound: %s", soundFile);

	for (new id = 1; id <= g_maxClients; id++)
	{
		if (!is_user_connected(id) || is_user_bot(id))
			continue;

		VTC_PlaySound(id, soundFile);
	}
}
