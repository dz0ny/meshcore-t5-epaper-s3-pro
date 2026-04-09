#pragma once

namespace ui::task {

// Start the UI task on the specified core.
// Must be called after board::init() (screen, touch initialized).
void start(int core);

} // namespace ui::task
