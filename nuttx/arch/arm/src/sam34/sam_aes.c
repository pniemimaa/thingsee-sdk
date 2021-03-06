/****************************************************************************
 * arch/arm/src/sam34/sam_aes.c
 *
 *   Copyright (C) 2014 Gregory Nutt. All rights reserved.
 *   Author:  Max Nekludov <macscomp@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/crypto/crypto.h>
#include <nuttx/arch.h>
#include <arch/board/board.h>

#include "up_internal.h"
#include "up_arch.h"

#include "chip.h"
#include "sam_periphclks.h"
#include "sam_aes.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AES_BLOCK_SIZE 16

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sem_t lock;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void aes_lock(void)
{
  sem_wait(&lock);
}

static void aes_unlock(void)
{
  sem_post(&lock);
}

static void aes_memcpy(void *out, const void *in, size_t size)
{
  size_t i;
  size_t wcount = size / 4;
  for (i = 0; i < wcount; i++, out = (uint8_t*)out + 4, in = (uint8_t*)in + 4)
    {
      *(uint32_t*)out = *(uint32_t*)in;
    }
}

static void aes_encryptblock(void *out, const void *in)
{
  aes_memcpy((void*)SAM_AES_IDATAR, in, AES_BLOCK_SIZE);

  putreg32(AES_CR_START, SAM_AES_CR);

  while(!(getreg32(SAM_AES_ISR) & AES_ISR_DATRDY)) {}

  aes_memcpy(out, (void*)SAM_AES_ODATAR, AES_BLOCK_SIZE);
}

static int aes_setup_mr(uint32_t keysize, int mode, int encrypt)
{
  uint32_t regval = AES_MR_SMOD_MANUAL_START | AES_MR_CKEY;

  if (encrypt)
    regval |= AES_MR_CIPHER_ENCRYPT;
  else
    regval |= AES_MR_CIPHER_DECRYPT;

  switch(keysize)
  {
  case 16:
    regval |= AES_MR_KEYSIZE_AES128;
    break;
  case 24:
    regval |= AES_MR_KEYSIZE_AES192;
    break;
  case 32:
    regval |= AES_MR_KEYSIZE_AES256;
    break;
  default:
    return -EINVAL;
  }

  switch(mode)
  {
  case AES_MODE_ECB:
    regval |= AES_MR_OPMOD_ECB;
    break;
  case AES_MODE_CBC:
    regval |= AES_MR_OPMOD_CBC;
    break;
  case AES_MODE_CTR:
    regval |= AES_MR_OPMOD_CTR;
    break;
  default:
    return -EINVAL;
  }

  putreg32(regval, SAM_AES_MR);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int aes_cypher(void *out, const void *in, uint32_t size, const void *iv,
               const void *key, uint32_t keysize, int mode, int encrypt)
{
  int res = OK;

  if (size % 16)
    return -EINVAL;

  aes_lock();

  res = aes_setup_mr(keysize, mode, encrypt);
  if (res)
    {
      aes_unlock();
      return res;
    }

  aes_memcpy((void*)SAM_AES_KEYWR, key, keysize);
  if (iv)
    {
      aes_memcpy((void*)SAM_AES_IVR, iv, AES_BLOCK_SIZE);
    }

  while (size)
    {
      aes_encryptblock(out, in);
      out = (char*)out + AES_BLOCK_SIZE;
      in  = (char*)in  + AES_BLOCK_SIZE;
      size -= AES_BLOCK_SIZE;
    }

  aes_unlock();
  return res;
}

int up_aesinitialize()
{
  sem_init(&lock, 0, 1);
  sam_aes_enableclk();
  putreg32(AES_CR_SWRST, SAM_AES_CR);
  return OK;
}
