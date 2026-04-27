/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    utilities_def.h — PersonalNode
  *
  * FIX : ajout des tâches UTIL_SEQ pour SOS, FALL, TX.
  *       Sans ces IDs, UTIL_SEQ_RegTask et UTIL_SEQ_SetTask plantent.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __UTILITIES_DEF_H__
#define __UTILITIES_DEF_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ── UTIL_LPM : IDs Low Power Manager ───────────────────────────────────── */
typedef enum
{
  CFG_LPM_APPLI_Id  = 0,
  CFG_LPM_RTC_Id    = 1,
  CFG_LPM_UART_TX_Id = 2,
  CFG_LPM_TCXO_WA_Id = 3,
  CFG_LPM_Id_NBR
} CFG_LPM_Id_t;

/* ── UTIL_SEQ : priorités ────────────────────────────────────────────────── */
typedef enum
{
  CFG_SEQ_Prio_0 = 0,
  CFG_SEQ_Prio_NBR
} CFG_SEQ_Prio_Id_t;

/* ── UTIL_SEQ : tâches PersonalNode ─────────────────────────────────────── */
typedef enum
{
  CFG_SEQ_Task_SOS  = 0,   /* envoi STATUS_SOS (bouton PA0)     */
  CFG_SEQ_Task_FALL = 1,   /* envoi STATUS_FALL (chute détectée) */
  CFG_SEQ_Task_TX   = 2,   /* envoi STATUS_OK périodique         */
  CFG_SEQ_Task_NBR
} CFG_SEQ_Task_Id_t;

/* ── UTIL_SEQ : événements (aucun) ──────────────────────────────────────── */
typedef enum
{
  CFG_SEQ_Evt_NBR
} CFG_SEQ_IdleEvt_Id_t;

#ifdef __cplusplus
}
#endif

#endif /* __UTILITIES_DEF_H__ */