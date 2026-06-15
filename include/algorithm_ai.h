#ifndef EBOT_ALGORITHM_AI_INCLUDED
#define EBOT_ALGORITHM_AI_INCLUDED

extern void AlgorithmAI_OnPlayerChat(edict_t* player, const char* text);
extern void AlgorithmAI_Think(void);
extern void AlgorithmAI_StartHelper(void);
extern void AlgorithmAI_StopHelper(void);

#endif // EBOT_ALGORITHM_AI_INCLUDED
