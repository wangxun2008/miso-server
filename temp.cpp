#include "miso_server.h"
#include <iostream>

int main() {
    try {
        miso::MisoServer server(53000, 54000);
        std::cout << "MisoServer started on TCP 53000, UDP 54000" << std::endl;
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
