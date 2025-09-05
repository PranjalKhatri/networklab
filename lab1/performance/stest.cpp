#include <iostream>
#include <cstdlib> // for system()
#include <string>

int main()
{
    for (int j = 1; j <= 10; j++)
    {
        std::string tmp = "echo > rs"+std::to_string(j)+".txt";
        system(tmp.c_str());
        for (int i = 1; i <= 32; i++)
        {
            std::string cmd = "./server udp 6969 1 " + std::to_string(i) + " >> rs"+std::to_string(j)+ ".txt";
            std::cout << "Executing: " << cmd << std::endl;
            int ret = std::system(cmd.c_str());
            if (ret != 0)
            {
                std::cerr << "Command failed with return code " << ret << std::endl;
                break;
            }
        }
    }
    return 0;
}
