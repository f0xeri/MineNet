//
// Created by Yaroslav on 19.07.2021.
//

#ifndef MINENET_MINENETSERVER_HPP
#define MINENET_MINENETSERVER_HPP

#include "MineNet.h"
#include "MineThreadSafeQueue.hpp"

namespace MineNet
{
    template<typename T>
    class IServer
    {
    public:
        IServer(const uint16_t port) : asioAcceptor(asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
        {

        }

        virtual ~IServer()
        {
            stop();
        }

        bool start()
        {
            try
            {
                awaitForClientConnection();
                threadContext = std::thread([this]() { asioContext.run(); });
            }
            catch (std::exception &e)
            {
                std::cerr << "[SERVER][ERROR] Exception: " << e.what() << "\n";
                return false;
            }

            std::cout << "[SERVER] Started!\n";
            return true;
        }

        void stop()
        {
            asioContext.stop();
            if (threadContext.joinable()) threadContext.join();
            std::cout << "[SERVER] Stopped!";
        }

        void awaitForClientConnection()
        {
            asioAcceptor.async_accept(
                    [this](std::error_code ec, asio::ip::tcp::socket socket)
                    {
                        if (!ec)
                        {
                            std::cout << "[SERVER] Incoming connection: " << socket.remote_endpoint() << "\n";
                            std::shared_ptr<connection<T>> newConnection = std::make_shared<connection<T>>(connection<T>::owner::server, asioContext, std::move(socket), queueMessageIn);
                            if (onClientConnect(newConnection))
                            {
                                dequeConnections.push_back(std::move(newConnection));
                                dequeConnections.back()->connectToClient(this, idCounter++);
                                std::cout << "[SERVER] Connection approved: " << dequeConnections.back()->getID() << " id.\n";
                            }
                            else
                            {
                                std::cout << "[SERVER] Connection denied." << "\n";
                            }
                        }
                        else
                        {
                            std::cerr << "[SERVER] New connection failed: " << ec.message() << "\n";
                        }
                        awaitForClientConnection();
                    });
        }

        void messageClient(std::shared_ptr<connection<T>> client, const message<T>& msg)
        {
            if (client && client->isConnected())
            {
                client->send(msg);
            }
            else
            {
                onClientDisconnect(client);
                client.reset();
                dequeConnections.erase(std::remove(dequeConnections.begin(), dequeConnections.end(), client), dequeConnections.end());
            }
        }

        void messageAllClients(const message<T>& msg, std::shared_ptr<connection<T>> ignoreClient = nullptr)
        {
            bool invalidClientExists = false;
            for (auto &client : dequeConnections)
            {
                if (client && client->isConnected())
                {
                    if (client != ignoreClient)
                        client->send(msg);
                }
                else
                {
                    onClientDisconnect(client);
                    client.reset();
                    invalidClientExists = true;
                }
            }
            if (invalidClientExists)
            {
                dequeConnections.erase(std::remove(dequeConnections.begin(), dequeConnections.end(), nullptr), dequeConnections.end());
            }
        }

        void update(size_t maxMessages = -1, bool wait = false)
        {
            if (wait) queueMessageIn.wait();
            size_t messageCount = 0;
            while (messageCount < maxMessages && !queueMessageIn.empty())
            {
                auto msg = queueMessageIn.pop_front();
                onMessage(msg.remote, msg.msg);
                messageCount++;
            }
        }

    protected:
        virtual bool onClientConnect(std::shared_ptr<connection<T>> client)
        {
            return false;
        }

        virtual void onClientDisconnect(std::shared_ptr<connection<T>> client)
        {

        }

        virtual void onMessage(std::shared_ptr<connection<T>> client, message<T> &message)
        {

        }
    public:
        virtual void onClientValidated(std::shared_ptr<connection<T>> client)
        {

        }
        tsqueue<owned_message<T>> queueMessageIn;
        asio::io_context asioContext;
        std::thread threadContext;
        std::deque<std::shared_ptr<connection<T>>> dequeConnections;
        asio::ip::tcp::acceptor asioAcceptor;
        uint32_t idCounter = 0;
    };
}

#endif //MINENET_MINENETSERVER_HPP
