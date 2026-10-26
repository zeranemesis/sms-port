#ifndef SMS_AP_BLUE_COINS_HPP
#define SMS_AP_BLUE_COINS_HPP
namespace smsap {
// Physical card-bool indices from original-world-contract.json. Retail allocates
// 50 bits per stage; Corona's actual pickups occupy indices 20..29.
inline bool blue_coin_bit(unsigned bit) {
  if (bit >= 120 && bit < 140) return true;
  for (unsigned start = 170; start <= 470; start += 50)
    if (bit >= start && bit < start + 30) return true;
  return bit >= 540 && bit < 550;
}
inline bool trade_shine_bit(unsigned bit) {
  return (bit >= 70 && bit <= 85) || (bit >= 108 && bit <= 115);
}
inline int trade_index(unsigned bit) {
  if (bit >= 70 && bit <= 85) return int(bit - 70);
  if (bit >= 108 && bit <= 115) return int(bit - 108 + 16);
  return -1;
}
inline bool nozzle_box_bit(unsigned bit) {
  return (bit >= 872 && bit <= 877) || bit == 882 || bit == 884 || bit == 885 || bit == 886;
}
inline bool location_bit(unsigned bit, int blue_mode, unsigned trades, bool nozzles = false) {
  if (nozzles && nozzle_box_bit(bit)) return true;
  if (blue_mode == 0) return bit < 119;
  int trade = trade_index(bit);
  if (trade >= 0) return unsigned(trade) < trades;
  return (blue_mode == 1 && blue_coin_bit(bit)) || bit < 119;
}
}
#endif
