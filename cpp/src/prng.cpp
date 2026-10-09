#include "prng.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace {

std::string jsonString(const std::string& text) {
  std::string out = "\"";
  for (unsigned char c : text) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
      out.push_back(static_cast<char>(c));
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else {
      out.push_back(static_cast<char>(c));
    }
  }
  out.push_back('"');
  return out;
}

std::string btoa(const std::string& text) {
  static const char table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  int value = 0;
  int bits = -6;
  for (unsigned char c : text) {
    value = (value << 8) + c;
    bits += 8;
    while (bits >= 0) {
      out.push_back(table[(value >> bits) & 63]);
      bits -= 6;
    }
  }
  if (bits > -6) {
    out.push_back(table[((value << 8) >> (bits + 8)) & 63]);
  }
  while (out.size() % 4) {
    out.push_back('=');
  }
  return out;
}

}  // namespace

double Prng::hash(const std::string& text) {
  const std::string encoded = btoa(jsonString(text));
  double z = 0.0;
  for (std::size_t i = 0; i < encoded.size(); ++i) {
    z += static_cast<unsigned char>(encoded[i]) * std::pow(128.0, static_cast<double>(i));
  }
  return z;
}

double Prng::seed(const std::string& text) {
  double y = 0.0;
  double z = 0.0;
  const double hashed = hash(text);
  do {
    y = std::fmod(hashed + z, m_);
    if (y < 0.0) {
      y += m_;
    }
    z += 1.0;
  } while (std::fmod(y, p_) == 0.0 || std::fmod(y, q_) == 0.0 || y == 0.0 || y == 1.0);
  s_ = y;
  const double initial = s_;
  for (int i = 0; i < 10; ++i) {
    next();
  }
  return initial;
}

double Prng::next() {
  s_ = std::fmod(s_ * s_, m_);
  if (s_ < 0.0) {
    s_ += m_;
  }
  return s_ / m_;
}
