/****************************************************************************
 * boards/arm/stm32h7/nucleo-h723vg/src/stm32_appinitialize.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/board.h>
#include <sys/types.h>

#include "mems-board-h723vg.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command BOARDIOC_INIT.
 *
 * Input Parameters:
 *   arg - The boardctl() argument is passed to the board_app_initialize()
 *         implementation without modification.  The argument has no
 *         meaning to NuttX; the meaning of the argument is a contract
 *         between the board-specific initialization logic and the
 *         matching application logic.  The value could be such things as a
 *         mode enumeration value, a set of DIP switch switch settings, a
 *         pointer to configuration data read from a file or serial FLASH,
 *         or whatever you would like to do with it.  Every implementation
 *         should accept zero/NULL as a default configuration.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure to indicate the nature of the failure.
 *
 ****************************************************************************/
#include "debug.h"
#include <errno.h>

// #include <nuttx/binfmt/builtin.h>
// #include <nuttx-apps/include/builtin/builtin.h>
#include <nuttx/lib/builtin.h>

struct nsh_param_s {
  /* Redirect input/output through `fd` OR `path_name`
   *
   * Select one:
   * 1. Using fd_in/fd_out as oldfd for dup2() if greater than -1.
   * 2. Using file_in/file_out as full path to the file if it is
   *    not NULL, and oflags_in/oflags_out as flags for open().
   */

  int fd_in;
  int fd_out;

  int oflags_in;
  int oflags_out;
  FAR const char *file_in;
  FAR const char *file_out;
};

extern int exec_builtin(FAR const char *appname, FAR char *const *argv,
                        FAR const struct nsh_param_s *param);

int board_app_initialize(uintptr_t arg) {
#ifdef CONFIG_BOARD_LATE_INITIALIZE
  /* Board initialization already performed by board_late_initialize() */
  pid_t pid;
  const char *argv[] = {"mems", NULL};
  int ret;

  _info("board_late_initialize: Starting mems...\n");

  /* 1. Инициализируем атрибуты для posix_spawn */
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);

  /* 2. Настраиваем планировщик и приоритет для нового приложения */
  struct sched_param param;
  param.sched_priority = 100;
  posix_spawnattr_setschedpolicy(&attr, SCHED_RR);
  posix_spawnattr_setschedparam(&attr, &param);

  /* 3. Запускаем задачу mems с использованием posix_spawnp */
  ret = posix_spawnp(&pid, "mems", NULL, &attr, (char *const *)argv, NULL);
  if (ret != 0) {
    _err("board_late_initialize: ERROR: posix_spawnp failed: %d\n", errno);
  } else {
    _info("board_late_initialize: mems started successfully with PID %d\n",
          pid);
  }

  /* 4. Освобождаем ресурсы атрибутов */
  posix_spawnattr_destroy(&attr);
#else
  /* Perform board-specific initialization */

  return stm32_bringup();
#endif
}
