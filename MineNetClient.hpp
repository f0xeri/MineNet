//
// Created by Yaroslav on 19.07.2021.
//

#ifndef MINENET_MINENETCLIENT_HPP
#define MINENET_MINENETCLIENT_HPP

#include "MineNet.h"
#include "MineThreadSafeQueue.hpp"

namespace MineNet
{
    template<typename T>
    class IClient
    {
    public:
        IClient() : socket(context) {}
        virtual ~IClient() { disconnect(); }

        bool connect(const std::string &host, const uint16_t port)
        {
            try
            {
                conn = std::make_unique<connection<T>>(
                        connection<T>::owner::client, context, asio::ip::tcp::socket(context), queueMessagesIn);
                asio::ip::tcp::resolver resolver(context);
                asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
                conn->connectToServer(endpoints);
                threadContext = std::thread([this]() { context.run(); });
            }
            catch (std::exception &e)
            {
                std::cerr << "[ERROR] Client exception: " << e.what() << "\n";
                return false;
            }
            return true;
        }

        void disconnect()
        {
            if (conn->isConnected())
            {
                conn->disconnect();
            }
            context.stop();
            if (threadContext.joinable())
                threadContext.join();

            conn.release();
        }

        bool isConnected()
        {
            if (conn)
                return conn->isConnected();
            else
                return false;
        }

        void send(const message<T>& msg)
        {
            if (isConnected())
                conn->send(msg);
        }

        tsqueue<owned_message<T>> &incoming()
        {
            return queueMessagesIn;
        }
    protected:
        asio::io_context context;
        std::thread threadContext;
        asio::ip::tcp::socket socket;
        std::unique_ptr<connection<T>> conn;

    private:
        tsqueue<owned_message<T>> queueMessagesIn;
    };
}

#endif //MINENET_MINENETCLIENT_HPP
