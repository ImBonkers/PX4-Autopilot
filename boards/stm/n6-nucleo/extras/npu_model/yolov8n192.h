/**
  ******************************************************************************
  * @file    yolov8n192.h
  * @author  STEdgeAI
  * @date    2026-03-26 17:13:51
  * @brief   Minimal description of the generated c-implemention of the network
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */
#ifndef LL_ATON_YOLOV8N192_H
#define LL_ATON_YOLOV8N192_H

/******************************************************************************/
#define LL_ATON_YOLOV8N192_C_MODEL_NAME        "yolov8n192"
#define LL_ATON_YOLOV8N192_ORIGIN_MODEL_NAME   "yolov8n_192_coco_person"

/************************** USER ALLOCATED IOs ********************************/
#define LL_ATON_YOLOV8N192_USER_ALLOCATED_INPUTS   (1)  // Number of input buffers not allocated by the compiler
#define LL_ATON_YOLOV8N192_USER_ALLOCATED_OUTPUTS  (1)  // Number of output buffers not allocated by the compiler

/************************** INPUTS ********************************************/
#define LL_ATON_YOLOV8N192_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_3_out_0
#define LL_ATON_YOLOV8N192_IN_1_ALIGNMENT   (32)
#define LL_ATON_YOLOV8N192_IN_1_SIZE_BYTES  (110592)

/************************** OUTPUTS *******************************************/
#define LL_ATON_YOLOV8N192_OUT_NUM        (1)    // Total number of output buffers
// Output buffer 1 -- Transpose_547_out_0
#define LL_ATON_YOLOV8N192_OUT_1_ALIGNMENT   (32)
#define LL_ATON_YOLOV8N192_OUT_1_SIZE_BYTES  (15120)

#endif /* LL_ATON_YOLOV8N192_H */
