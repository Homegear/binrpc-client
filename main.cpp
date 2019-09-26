#include <homegear-base/BaseLib.h>
#include <iostream>

void printHelp()
{
    std::cout << "Usage: binrpc [OPTIONS] -m [METHOD] -p [PARAMETERS]" << std::endl << std::endl;
    std::cout << "OPTIONS:" << std::endl;
    std::cout << "  Option              Meaning" << std::endl;
    std::cout << "  -h                  Show this help" << std::endl;
    std::cout << "  -H                  Hostname/IP address to connect to" << std::endl;
    std::cout << "  -P                  Port to connect to" << std::endl;
    std::cout << "  -v                  Print program version" << std::endl;
    std::cout << std::endl;
    std::cout << "METHOD: The RPC method name" << std::endl;
    std::cout << "PARAMETERS: The RPC parameters as JSON array. E. g. '[\"string\", 5.2, true]'" << std::endl;

}

int main(int argc, char* argv[])
{
    try
    {
        auto bl = std::make_shared<BaseLib::SharedObjects>();

        std::string hostname = "localhost";
        uint16_t port = 0;

        std::string method;
        std::string parametersString;

        int option = -1;
        while((option = getopt(argc, argv, ":hH:P:m:p:")) != -1)
        {
            switch(option)
            {
                case 'h':
                    printHelp();
                    exit(0);
                case 'H':
                    hostname = std::string(optarg);
                    break;
                case 'P':
                    port = BaseLib::Math::getUnsignedNumber(std::string(optarg));
                    break;
                case 'm':
                    method = std::string(optarg);
                    break;
                case 'p':
                    parametersString = std::string(optarg);
                    break;
                case '?':
                    std::cerr << "Unknown option: " << (char)optopt << std::endl;
                    printHelp();
                    exit(1);
            }
        }

        if(hostname.empty() || port == 0)
        {
            std::cerr << "Hostname or port are invalid or not specified." << std::endl;
            printHelp();
            exit(1);
        }

        if(method.empty())
        {
            std::cerr << "No method specified." << std::endl;
            printHelp();
            exit(1);
        }

        auto parameters = parametersString.empty() ? std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray) : BaseLib::Rpc::JsonDecoder::decode(parametersString);
        if(parameters->type != BaseLib::VariableType::tArray)
        {
            std::cerr << "Parameters are no JSON array." << std::endl;
            exit(1);
        }

        auto socket = std::make_shared<BaseLib::TcpSocket>(bl.get(), hostname, std::to_string(port));
        socket->open();

        BaseLib::Rpc::RpcEncoder rpcEncoder(bl.get(), false, true);
        BaseLib::Rpc::RpcDecoder rpcDecoder(bl.get(), false, false);
        std::vector<char> packet;
        rpcEncoder.encodeRequest(method, parameters->arrayValue, packet);
        socket->proofwrite(packet);

        BaseLib::Rpc::BinaryRpc rpc(bl.get());

        std::array<char, 4096> buffer{};
        while(true)
        {
            auto result = socket->proofread(buffer.data(), buffer.size());
            if(result == 0) continue;
            else if(result > buffer.size()) continue;

            rpc.process(buffer.data(), result);
            if(rpc.isFinished())
            {
                if(rpc.getType() == BaseLib::Rpc::BinaryRpc::Type::response)
                {
                    auto response = rpcDecoder.decodeResponse(rpc.getData(), 0);
                    std::string json;
                    BaseLib::Rpc::JsonEncoder::encode(response, json);
                    std::cout << json << std::endl;
                    break;
                }
            }
        }

        return 0;
    }
    catch(const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
    }
    return 1;
}