#include <boost/format.hpp>
#include <iostream>

int main(int argc, char *argv[]) {
  std::cout << boost::format("%2% %1%") % 3 % std::string("Hello") << std::endl;
  return 0;
}
