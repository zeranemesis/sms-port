#ifndef SMS_AP_CONSUMABLES_HPP
#define SMS_AP_CONSUMABLES_HPP
namespace smsap {
// The applied cursor lives in the same native save as the life count. Reloading
// an older save replays only receipts absent from that save, never spent lives.
inline int one_up_delta(unsigned accepted, unsigned applied) {
  return accepted >= applied ? int(accepted - applied) : -1;
}
} // namespace smsap
#endif
