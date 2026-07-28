#include "pch.h"
#include "SystemVolumeService.h"

static_assert(media::ClampSystemVolumeLevel(-0.25F) == 0.0F);
static_assert(media::ClampSystemVolumeLevel(0.50F) == 0.50F);
static_assert(media::ClampSystemVolumeLevel(1.25F) == 1.0F);
