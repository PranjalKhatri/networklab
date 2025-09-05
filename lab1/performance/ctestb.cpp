#include <iostream>
#include <cstdlib> // for system()
#include <string>
#include <thread> // for sleep_for
#include <chrono> // for milliseconds

int main()
{

    for (int j = 1; j <= 10; j++)
    {
        std::string tmp = "echo > rc" + std::to_string(j) + ".txt";
        system(tmp.c_str());
        for (int i = 1; i <= 10; i++)
        {
            std::string cmd = "./client tcp 172.16.114.99 6969 "+std::to_string(1<<(i-1))+" " + std::to_string(1024*10) + " >> rc"+std::to_string(j)+ ".txt";
            std::cout << "Executing: " << cmd << std::endl;

            int ret = std::system(cmd.c_str());
            if (ret != 0)
            {
                std::cerr << "Command failed with return code " << ret << std::endl;
                break;
            }

            // wait 100 ms between executions
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    return 0;
}
