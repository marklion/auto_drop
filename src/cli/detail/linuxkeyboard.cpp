#include "linuxkeyboard.h"

void cli::detail::AD_KEYBOARD_EVENT_NODE::handleEvent()
{
    m_lkb->Read();
}