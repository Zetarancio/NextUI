#pragma once

#include <utility>

// After a verified NAND write, this item must not run that write again.
// Replace its confirm callback, then enter the power submenu.
// Back only clears the deferred flag; the next A uses the new callback.
template <class Item, class Callback>
void detach_verified_write(Item &item, Callback reopen)
{
	item.setConfirmCallback(std::move(reopen));
	item.setDesc("Write already verified. Choose Shut down or Restart.");
	item.defer(true);
}
