//
// Created by Yaroslav on 19.07.2021.
//

#ifndef MINENET_MINENETCONNECTION_HPP
#define MINENET_MINENETCONNECTION_HPP

#include "MineNet.h"
#include "MineThreadSafeQueue.hpp"
#include "MineNetServer.hpp"

namespace MineNet
{
    template<typename T>
    class connection : public std::enable_shared_from_this<connection<T>>
    {
    public:

        enum class owner
        {
            server,
            client
        };

        connection(owner parent, asio::io_context &asioConetext, asio::ip::tcp::socket socket, tsqueue<owned_message<T>> &queueIn)
            : asioContext(asioConetext), socket(std::move(socket)), queueMessagesIn(queueIn)
        {
            ownerType = parent;
            if (ownerType == owner::server)
            {
                handshakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
                handshakeCheck = encrypt(handshakeOut);
            }
            else
            {
                handshakeIn = 0;
                handshakeOut = 0;
            }
        }
        virtual ~connection(){}

    public:

        uint32_t getID()
        {
            return id;
        }

        void connectToClient(MineNet::IServer<T>* server, uint32_t uid = 0)
        {
            if (ownerType == owner::server)
            {
                if (socket.is_open())
                {
                    id = uid;
                    writeValidation();
                    readValidation(server);
                }
            }
        }

        void connectToServer(const asio::ip::tcp::resolver::results_type& endpoints)
        {
            if (ownerType == owner::client)
            {
                asio::async_connect(socket, endpoints,
                                    [this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
                                    {
                                        if (!ec)
                                        {
                                            readValidation();
                                        }
                                        else
                                        {
                                            std::cout << "[FAIL] Connection failed.\n";
                                            socket.close();
                                        }
                                    });
            }
        }

        void disconnect()
        {
            if (isConnected())
                asio::post(asioContext, [this]() { socket.close(); });
        }

        bool isConnected() const
        {
            return socket.is_open();
        }

        void send(const message<T>& msg)
        {
            asio::post(asioContext,
                       [this, msg]()
                       {
                           bool isWritingMessage = !queueMessagesOut.empty();
                           queueMessagesOut.push_back(msg);
                           if (!isWritingMessage)
                           {
                               writeHeader();
                           }
                       });
        }

    private:

        void addToIncomingMessageQueue()
        {
            if(ownerType == owner::server)
                queueMessagesIn.push_back({ this->shared_from_this(), tempMessageIn });
            else
                queueMessagesIn.push_back({ nullptr, tempMessageIn });
            readHeader();
        }

        void readHeader()
        {
            asio::async_read(socket, asio::buffer(&tempMessageIn.header, sizeof(message_header<T>)),
                             [this](std::error_code ec, std::size_t length)
                             {
                                 if (!ec)
                                 {
                                    if (tempMessageIn.header.size > 0)
                                    {
                                        tempMessageIn.body.resize(tempMessageIn.header.size - 8);
                                        readBody();
                                    }
                                    else
                                    {
                                        addToIncomingMessageQueue();
                                    }
                                 }
                                 else
                                 {
                                     std::cout << "[FAIL] Read header failed: " << id << " id.\n";
                                     socket.close();
                                 }
                             });
        }

        void readBody()
        {
            asio::async_read(socket, asio::buffer(tempMessageIn.body.data(), tempMessageIn.body.size()),
                             [this](std::error_code ec, std::size_t bytes_transferred)
                             {
                                     if (!ec)
                                     {
                                         addToIncomingMessageQueue();
                                     }
                                     else
                                     {
                                         std::cout << "[" << id << "] Read Body Fail, trasnfered " << bytes_transferred << " bytes.\n";
                                         socket.close();
                                     }
                             });
        }

        void writeHeader()
        {
            asio::async_write(socket, asio::buffer(&queueMessagesOut.front().header, sizeof(message_header<T>)),
                             [this](std::error_code ec, std::size_t length)
                             {
                                 if (!ec)
                                 {
                                     if (queueMessagesOut.front().body.size() > 0)
                                     {
                                         writeBody();
                                     }
                                     else
                                     {
                                         queueMessagesOut.pop_front();
                                         if (!queueMessagesOut.empty())
                                         {
                                             writeHeader();
                                         }
                                     }
                                 }
                                 else
                                 {
                                     std::cout << "[FAIL] Write header failed: " << id << " id.\n";
                                     socket.close();
                                 }
                             });
        }

        void writeBody()
        {
            asio::async_write(socket, asio::buffer(queueMessagesOut.front().body.data(), queueMessagesOut.front().body.size()),
                              [this](std::error_code ec, std::size_t length)
                              {
                                  if (!ec)
                                  {
                                      queueMessagesOut.pop_front();
                                      if (!queueMessagesOut.empty())
                                      {
                                          writeHeader();
                                      }
                                  }
                                  else
                                  {
                                      std::cout << "[FAIL] Write body failed: " << id << " id.\n";
                                      socket.close();
                                  }
                              });
        }

        uint64_t encrypt(uint64_t input)
        {
            uint64_t out = input ^ 0xDEADBEEFC0DECAFE;
            out = (out & 0xF0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F) << 4;
            return out ^ 0xFACE8D0012;
        }

        void writeValidation()
        {
            asio::async_write(socket, asio::buffer(&handshakeOut, sizeof(uint64_t)),
                              [this](std::error_code ec, std::size_t length)
                              {
                                  if (!ec)
                                  {
                                      // Validation data sent, clients should sit and wait
                                      // for a response (or a closure)
                                      if (ownerType == owner::client)
                                          readHeader();
                                  }
                                  else
                                  {
                                      socket.close();
                                  }
                              });
        }

        void readValidation(MineNet::IServer<T>* server = nullptr)
        {
            asio::async_read(socket, asio::buffer(&handshakeIn, sizeof(uint64_t)),
                             [this, server](std::error_code ec, std::size_t length)
                             {
                                 if (!ec)
                                 {
                                     if (ownerType == owner::server)
                                     {
                                         // Connection is a server, so check response from client

                                         // Compare sent data to actual solution
                                         if (handshakeIn == handshakeCheck)
                                         {
                                             // Client has provided valid solution, so allow it to connect properly
                                             std::cout << "[INFO] Client validated." << std::endl;
                                             server->onClientValidated(this->shared_from_this());

                                             // Sit waiting to receive data now
                                             readHeader();
                                         }
                                         else
                                         {
                                             // Client gave incorrect data, so disconnect
                                             std::cout << "[INFO] Client disconnected (fail validation)." << std::endl;
                                             socket.close();
                                         }
                                     }
                                     else
                                     {
                                         // Connection is a client, so solve puzzle
                                         handshakeOut = encrypt(handshakeIn);

                                         // Write the result
                                         writeValidation();
                                     }
                                 }
                                 else
                                 {
                                     // Some biggerfailure occured
                                     std::cout << "[INFO] Client disconnected (readValidation)." << std::endl;
                                     socket.close();
                                 }
                             });
        }

    protected:
        asio::ip::tcp::socket socket;
        asio::io_context &asioContext;

        MineNet::tsqueue<message<T>> queueMessagesOut;
        MineNet::tsqueue<owned_message<T>> &queueMessagesIn;
        message<T> tempMessageIn;
        owner ownerType = owner::server;
        uint32_t id = 0;

        uint64_t handshakeOut = 0;
        uint64_t handshakeIn = 0;
        uint64_t handshakeCheck = 0;

    };
}

#endif //MINENET_MINENETCONNECTION_HPP
