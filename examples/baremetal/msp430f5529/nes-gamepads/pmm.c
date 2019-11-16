/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of Qbs.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "hwdefs.h"
#include "pmm.h"

#define PMMCTL0_H_ADDRESS (PMM_BASE + OFS_PMMCTL0_H)
#define PMMCTL0_L_ADDRESS (PMM_BASE + OFS_PMMCTL0_L)
#define PMMRIE_ADDRESS (PMM_BASE + OFS_PMMRIE)
#define PMMIFG_ADDRESS (PMM_BASE + OFS_PMMIFG)
#define SVSMHCTL_ADDRESS (PMM_BASE + OFS_SVSMHCTL)
#define SVSMLCTL_ADDRESS (PMM_BASE + OFS_SVSMLCTL)

#define PMM_OTHER_BITSL \
    (SVSLRVL0 | SVSLRVL1 | SVSMLRRL0 | SVSMLRRL1 | SVSMLRRL2)

#define PMM_OTHER_BITSH \
    (SVSHRVL0 | SVSHRVL1 | SVSMHRRL0 | SVSMHRRL1 | SVSMHRRL2)

#define PMM_CLEANUP_FLAGS \
    (SVMHVLRIFG | SVMHIFG | SVSMHDLYIFG | SVMLVLRIFG | SVMLIFG | SVSMLDLYIFG)

static void pmm_write_access_allow(bool allow)
{
    HWREG8(PMMCTL0_H_ADDRESS) = allow ? PMMPW_H : 0;
}

static void pmm_interrupt_flags_clear(uint16_t flags)
{
    HWREG16(PMMIFG_ADDRESS) &= ~flags;
}

static void pmm_interrupt_flags_wait(uint16_t flags)
{
    while ((HWREG16(PMMIFG_ADDRESS) & flags) == 0) {
        ;
    }
}

static uint8_t pmm_level_mask(enum ppm_voltage voltage)
{
    switch (voltage) {
    case PMM_VOLTAGE_1_35V:
        return PMMCOREV_0;
    case PMM_VOLTAGE_1_55V:
        return PMMCOREV_1;
    case PMM_VOLTAGE_1_75V:
        return PMMCOREV_2;
    case PMM_VOLTAGE_1_85V:
        return PMMCOREV_3;
    default:
        break;
    }
    return PMMCOREV_0;
}

static bool pmm_voltage_set_up(uint8_t level)
{
    pmm_write_access_allow(true);

    // Disable interrupts and backup all registers.
    uint16_t pmmrie_backup = HWREG16(PMMRIE_ADDRESS);
    HWREG16(PMMRIE_ADDRESS) &= ~(SVMHVLRPE | SVSHPE | SVMLVLRPE | SVSLPE
                                 | SVMHVLRIE | SVMHIE | SVSMHDLYIE | SVMLVLRIE
                                 | SVMLIE | SVSMLDLYIE);
    uint16_t svsmhctl_backup = HWREG16(SVSMHCTL_ADDRESS);
    uint16_t svsmlctl_backup = HWREG16(SVSMLCTL_ADDRESS);

    // Clear all interrupt flags.
    pmm_interrupt_flags_clear(0xFFFF);

    // Set SVM high side to new level and check if voltage increase is possible.
    HWREG16(SVSMHCTL_ADDRESS) = SVMHE | SVSHE | (SVSMHRRL0 * level);
    pmm_interrupt_flags_wait(SVSMHDLYIFG);
    pmm_interrupt_flags_clear(SVSMHDLYIFG);

    // Check if a voltage increase is possible.
    if ((HWREG16(PMMIFG_ADDRESS) & SVMHIFG) == SVMHIFG) {
        // Vcc is too low for a voltage increase, recover the previous settings.
        HWREG16(PMMIFG_ADDRESS) &= ~SVSMHDLYIFG;
        HWREG16(SVSMHCTL_ADDRESS) = svsmhctl_backup;
        pmm_interrupt_flags_wait(SVSMHDLYIFG);
        pmm_interrupt_flags_clear(PMM_CLEANUP_FLAGS);
        // Restore interrupt enable register.
        HWREG16(PMMRIE_ADDRESS) = pmmrie_backup;
        pmm_write_access_allow(false);
        return false;
    }

    // Set SVS high side to new level.
    HWREG16(SVSMHCTL_ADDRESS) |= (SVSHRVL0 * level);
    pmm_interrupt_flags_wait(SVSMHDLYIFG);
    pmm_interrupt_flags_clear(SVSMHDLYIFG);

    // Set new voltage level.
    HWREG8(PMMCTL0_L_ADDRESS) = PMMCOREV0 * level;

    // Set SVM, SVS low side to new level.
    HWREG16(SVSMLCTL_ADDRESS) = SVMLE | (SVSMLRRL0 * level)
            | SVSLE | (SVSLRVL0 * level);
    pmm_interrupt_flags_wait(SVSMLDLYIFG);
    pmm_interrupt_flags_clear(SVSMLDLYIFG);

    // Restore low side settings and clear all other bits.
    HWREG16(SVSMLCTL_ADDRESS) &= PMM_OTHER_BITSL;
    // Clear low side level settings in backup register and keep all other bits.
    svsmlctl_backup &= ~PMM_OTHER_BITSL;
    // Restore low side SVS monitor settings.
    HWREG16(SVSMLCTL_ADDRESS) |= svsmlctl_backup;

    // Restore high side settings and clear all other bits.
    HWREG16(SVSMHCTL_ADDRESS) &= PMM_OTHER_BITSH;
    // Clear high side level settings in backup register and keep all other bits.
    svsmhctl_backup &= ~PMM_OTHER_BITSH;
    // Restore high side SVS monitor settings.
    HWREG16(SVSMHCTL_ADDRESS) |= svsmhctl_backup;

    pmm_interrupt_flags_wait(SVSMLDLYIFG | SVSMHDLYIFG);
    pmm_interrupt_flags_clear(PMM_CLEANUP_FLAGS);

    // Restore interrupt enable register.
    HWREG16(PMMRIE_ADDRESS) = pmmrie_backup;
    pmm_write_access_allow(false);

    return true;
}

static uint16_t pmm_voltage_set_down(uint8_t level)
{
    pmm_write_access_allow(true);

    // Disable interrupts and backup all registers.
    uint16_t pmmrie_backup = HWREG16(PMMRIE_ADDRESS);
    HWREG16(PMMRIE_ADDRESS) &= ~(SVMHVLRPE | SVSHPE | SVMLVLRPE |
                                        SVSLPE | SVMHVLRIE | SVMHIE |
                                        SVSMHDLYIE | SVMLVLRIE | SVMLIE |
                                        SVSMLDLYIE
                                        );
    uint16_t svsmhctl_backup = HWREG16(SVSMHCTL_ADDRESS);
    uint16_t svsmlctl_backup = HWREG16(SVSMLCTL_ADDRESS);

    pmm_interrupt_flags_clear(SVMHIFG | SVSMHDLYIFG | SVMLIFG | SVSMLDLYIFG);

    // Set SVM, SVS high & low side to new settings in normal mode.
    HWREG16(SVSMHCTL_ADDRESS) = SVMHE | (SVSMHRRL0 * level) | SVSHE | (SVSHRVL0 * level);
    HWREG16(SVSMLCTL_ADDRESS) = SVMLE | (SVSMLRRL0 * level) | SVSLE | (SVSLRVL0 * level);

    pmm_interrupt_flags_wait(SVSMHDLYIFG | SVSMLDLYIFG);
    pmm_interrupt_flags_clear(SVSMHDLYIFG | SVSMLDLYIFG);

    // Set new voltage level.
    HWREG8(PMMCTL0_L_ADDRESS) = PMMCOREV0 * level;

    // Restore low side settings and clear all other bits.
    HWREG16(SVSMLCTL_ADDRESS) &= PMM_OTHER_BITSL;
    // Clear low side level settings in backup register and keep all other bits.
    svsmlctl_backup &= ~PMM_OTHER_BITSL;
    //Restore low side SVS monitor settings.
    HWREG16(SVSMLCTL_ADDRESS) |= svsmlctl_backup;

    // Restore high side settings and clear all other bits.
    HWREG16(SVSMHCTL_ADDRESS) &= PMM_OTHER_BITSH;
    // Clear high side level settings in backup register and keep all other bits.
    svsmhctl_backup &= ~PMM_OTHER_BITSH;
    // Restore high side SVS monitor settings.
    HWREG16(SVSMHCTL_ADDRESS) |= svsmhctl_backup;

    pmm_interrupt_flags_wait(SVSMLDLYIFG | SVSMHDLYIFG);
    pmm_interrupt_flags_clear(PMM_CLEANUP_FLAGS);

    // Restore interrupt enable register.
    HWREG16(PMMRIE_ADDRESS) = pmmrie_backup;
    pmm_write_access_allow(false);

    return true;
}

bool pmm_voltage_init(enum ppm_voltage voltage)
{
    const uint8_t exp_level = pmm_level_mask(voltage) & PMMCOREV_3;
    uint8_t act_level = (HWREG16(PMM_BASE + OFS_PMMCTL0) & PMMCOREV_3);
    bool result = true;
    while ((exp_level != act_level) && result) {
        if (exp_level > act_level)
            result = pmm_voltage_set_up(++act_level);
        else
            result = pmm_voltage_set_down(--act_level);
    }
    return result;
}
