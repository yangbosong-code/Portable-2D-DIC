#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::ostringstream output;
    output << stream.rdbuf();
    return output.str();
}

}  // namespace

int main() {
    const std::filesystem::path root(P2DIC_SOURCE_DIR);
    const auto service = read(root / "deploy/jetson/systemd/p2dic-edge.service");
    const auto timer = read(root / "deploy/jetson/systemd/p2dic-healthcheck.timer");
    const auto installer = read(root / "deploy/jetson/install.sh");
    if (service.find("Restart=on-failure") == std::string::npos ||
        service.find("KillSignal=SIGTERM") == std::string::npos ||
        service.find("ReadWritePaths=/var/lib/p2dic") == std::string::npos ||
        timer.find("OnUnitActiveSec=30") == std::string::npos ||
        installer.find("Keeping existing /etc/p2dic/dic-edge.conf") == std::string::npos ||
        installer.find("unresolved runtime libraries") == std::string::npos ||
        installer.find("systemctl enable p2dic-edge.service") == std::string::npos) {
        std::cerr << "Jetson deployment safety contract is incomplete\n";
        return 1;
    }
    return 0;
}
