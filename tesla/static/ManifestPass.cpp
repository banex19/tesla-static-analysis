#include "ManifestPass.h"

#include <sstream>

using std::string;
using std::stringstream;

namespace tesla {

const string ManifestPass::Error(string message) const {
  return PrefixMessage("Error", message);
}

const string ManifestPass::Debug(string message) const {
  return PrefixMessage("Debug", message);
}

const string ManifestPass::PrefixMessage(string pre, string message) const {
  stringstream ss;
  ss << pre << " (" << PassName() << "): " << message;
  return ss.str();
}

}
