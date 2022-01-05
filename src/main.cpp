#include <homegear-base/BaseLib.h>
#include <iostream>
#include <getopt.h>

void printHelp() {
  std::cout << "Usage: binrpc [OPTIONS] -m [METHOD] -p [PARAMETERS]" << std::endl << std::endl;
  std::cout << "OPTIONS:" << std::endl;
  std::cout << "  Option                    Long option     Meaning" << std::endl;
  std::cout << "  -h                        --help          Show this help" << std::endl;
  std::cout << "  -H <hostname>                             Hostname/IP address to connect to" << std::endl;
  std::cout << "  -P <port>                                 Port to connect to" << std::endl;
  std::cout << "  -t <timeout>                              Read timeout in milliseconds" << std::endl;
  std::cout << "  -e                        --tls           Encrypt communication" << std::endl;
  std::cout << "  -a <CA certificate file>  --ca            Path to PEM encoded CA certificate the server's certificate was signed with." << std::endl;
  std::cout << "                                            When not specified, the system's CA store is used." << std::endl;
  std::cout << "  -c <Certificate file>     --cert          Path to PEM encoded public certificate to login into the server" << std::endl;
  std::cout << "  -k <Private key file>     --key           Path to PEM encoded private certificate to login into the server" << std::endl;
  std::cout << "  -d <RPC hex>              --decode        Decode RPC hex" << std::endl;
  std::cout << "  -v                        --version       Print program version" << std::endl;
  std::cout << std::endl;
  std::cout << "METHOD: The RPC method name" << std::endl;
  std::cout << "PARAMETERS: The RPC parameters as JSON array. E. g. '[\"string\", 5.2, true]'" << std::endl;

}

int main(int argc, char *argv[]) {
  try {
    auto bl = std::make_shared<BaseLib::SharedObjects>();

    std::string hostname = "localhost";
    uint16_t port = 0;

    std::string method;
    std::string parametersString;
    int64_t timeout = -1;
    bool tls = false;
    std::string ca;
    std::string cert;
    std::string key;

    std::string decode;

    option longOptions[] = {
        {"help", 0, nullptr, 'h'},
        {"tls", 0, nullptr, 'e'},
        {"ca", 1, nullptr, 'a'},
        {"cert", 1, nullptr, 'c'},
        {"key", 1, nullptr, 'k'},
        {"version", 0, nullptr, 'v'},
        {"decode", 1, nullptr, 'd'},
        nullptr
    };

    int option = -1;
    while ((option = getopt_long(argc, argv, ":hH:P:m:p:t:ea:c:k:d:", longOptions, nullptr)) != -1) {
      switch (option) {
        case 'h': {
          printHelp();
          exit(0);
        }
        case 'H': {
          hostname = std::string(optarg);
          break;
        }
        case 'P': {
          port = BaseLib::Math::getUnsignedNumber(std::string(optarg));
          break;
        }
        case 'm': {
          method = std::string(optarg);
          break;
        }
        case 'p': {
          parametersString = std::string(optarg);
          break;
        }
        case 't': {
          timeout = BaseLib::Math::getUnsignedNumber(std::string(optarg));
          break;
        }
        case 'd': {
          decode = std::string(optarg);
        }
        case 'e': {
          tls = true;
          break;
        }
        case 'a': {
          ca = std::string(optarg);
          break;
        }
        case 'c': {
          cert = std::string(optarg);
          break;
        }
        case 'k': {
          key = std::string(optarg);
          break;
        }
        case '?': {
          std::cerr << "Unknown option: " << (char)optopt << std::endl;
          printHelp();
          exit(1);
        }
        default: {
          printHelp();
          exit(1);
        }
      }
    }

    if (!decode.empty()) {
      BaseLib::Rpc::RpcDecoder decoder;
      if (decode.compare(0, 8, "42696E00") == 0) { //request
        std::string methodName;
        auto parameters = decoder.decodeRequest(BaseLib::HelperFunctions::getUBinary(decode), methodName);
        auto request = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
        request->structValue->emplace("method", std::make_shared<BaseLib::Variable>(methodName));
        request->structValue->emplace("params", std::make_shared<BaseLib::Variable>(parameters));
        std::cout << BaseLib::Rpc::JsonEncoder::encode(request) << std::endl;
      } else {
        std::cout << BaseLib::Rpc::JsonEncoder::encode(decoder.decodeResponse(BaseLib::HelperFunctions::getUBinary(decode))) << std::endl;
      }
      exit(0);
    }

    if (port == 0) {
      std::cerr << "Port is invalid or not specified." << std::endl;
      printHelp();
      exit(1);
    }

    if (method.empty()) {
      std::cerr << "No method specified." << std::endl;
      printHelp();
      exit(1);
    }

    auto parameters = parametersString.empty() ? std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray) : BaseLib::Rpc::JsonDecoder::decode(parametersString);
    if (parameters->type != BaseLib::VariableType::tArray) {
      std::cerr << "Parameters are no JSON array." << std::endl;
      exit(1);
    }

    std::shared_ptr<BaseLib::TcpSocket> socket;
    if (!tls) {
      socket = std::make_shared<BaseLib::TcpSocket>(bl.get(), hostname, std::to_string(port));
    } else if (cert.empty() || key.empty()) {
      socket = std::make_shared<BaseLib::TcpSocket>(bl.get(), hostname, std::to_string(port), true, ca, true);
    } else {
      socket = std::make_shared<BaseLib::TcpSocket>(bl.get(), hostname, std::to_string(port), true, ca, true, cert, key);
    }
    if (timeout > 0) socket->setReadTimeout(timeout * 1000);
    socket->open();

    BaseLib::Rpc::RpcEncoder rpcEncoder(bl.get(), false, true);
    BaseLib::Rpc::RpcDecoder rpcDecoder(bl.get(), false, false);
    std::vector<char> packet;
    rpcEncoder.encodeRequest(method, parameters->arrayValue, packet);
    socket->proofwrite(packet);

    BaseLib::Rpc::BinaryRpc rpc(bl.get());

    std::array<char, 4096> buffer{};
    while (true) {
      auto bytesReceived = socket->proofread(buffer.data(), buffer.size());
      if (bytesReceived > (signed)buffer.size()) bytesReceived = buffer.size(); //Can't happen, but mandatory check for rpc.process

      bool breakLoop = false;
      int32_t bytesProcessed = 0;
      while (bytesProcessed < bytesReceived) {
        bytesProcessed += rpc.process(buffer.data() + bytesProcessed, bytesReceived - bytesProcessed);
        if (rpc.isFinished()) {
          if (rpc.getType() == BaseLib::Rpc::BinaryRpc::Type::response) {
            auto response = rpcDecoder.decodeResponse(rpc.getData(), 0);
            std::string json;
            BaseLib::Rpc::JsonEncoder::encode(response, json);
            std::cout << json << std::endl;
            breakLoop = true;
            break;
          }
        }
      }
      if (breakLoop) break;
    }

    return 0;
  }
  catch (const std::exception &ex) {
    std::cerr << ex.what() << std::endl;
  }
  return 1;
}