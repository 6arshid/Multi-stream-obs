/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/adapter.hpp"

namespace ums {

/* Manual / generic adapters (manual-adapter.cpp) */
AdapterPtr createManualAdapter();   /* id: "custom" */
AdapterPtr createTikTokAdapter();
AdapterPtr createInstagramAdapter();
AdapterPtr createXAdapter();
AdapterPtr createLinkedInAdapter();
AdapterPtr createRumbleAdapter();
AdapterPtr createDLiveAdapter();
AdapterPtr createBigoAdapter();      /* informational */
AdapterPtr createDiscordAdapter();   /* informational */
AdapterPtr createStreamYardAdapter();/* informational */

/* API adapters */
AdapterPtr createYouTubeAdapter();
AdapterPtr createTwitchAdapter();
AdapterPtr createKickAdapter();
AdapterPtr createVimeoAdapter();
AdapterPtr createFacebookAdapter();
AdapterPtr createRestreamAdapter();
AdapterPtr createTrovoAdapter();

} // namespace ums
