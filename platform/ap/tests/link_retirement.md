# Transient link command retirement

A valid command may be published while a ready snapshot is current, then wait
through a menu/transition until its payload expires. Retire its increasing
sequence after validating magic, AP session, native save and current boot.
Freshness determines whether a death queues; it must not prevent acknowledgment.
Far-future payloads are likewise retired without a death. Wrong identities or
boots and repeated sequences cannot advance the cursor.

The integration's production two-slot `tools/links_smoke.py` regression models
expired and far-future command timestamps at the native read boundary while
paused, then verifies that a fresh real AP DeathLink applies without restarting
the native process. It also checks wrong boot/slot/save and duplicate commands.
The preserved old native fails the expired-command acknowledgment assertion;
the fixed production bridge passes. Physical/readiness/respawn callbacks are
substituted; this does not claim a physical game transition or campaign.
