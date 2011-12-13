//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2011 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

//#include "client45000.h"

using boost::asio::ip::tcp;

class session
{
public:
  session(boost::asio::io_service& io_service)
    : socket_(io_service), isData_(false), isQuit_(false)

  {
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void start()
  {
    std::ostream response_stream(&response_);

    response_stream << "220 ukvpc.itandem.ru SMTP is glad to see you!\n";
    boost::asio::async_write(socket_, response_,
		boost::bind(&session::handle_write, this,
			boost::asio::placeholders::error));
  }

  void handle_read(const boost::system::error_code& error)
  {
    if (!error)
    {
      std::istream request_stream(&request_);
      std::ostream response_stream(&response_);
      std::string message;

      if(isData_) {
	response_stream << "250 769947 message accepted for delivery\n";
	// save data
	char *uniqname = strdup("./spool/msg.XXXXXX");
	mkstemp(uniqname);
	
	std::ofstream outfile(uniqname, std::ios::out | std::ios::binary);
	if(outfile) {
	  if (response_.size() > 0)
	    outfile << &request_;
	  outfile.close();
	}

	// send notice like ADM-CID
	// system("./sendNotice localhost 45000");

	isData_ = false;
      }
      else {
	std::string cmd;
	request_stream >> cmd;
	std::getline(request_stream, message);
	std::cout << cmd << message << std::endl;
	
	if(cmd=="HELO" ||
	   cmd=="EHLO" ||
	   cmd=="MAIL" ||
	   cmd=="RCPT" ||
	   cmd=="NOOP") {
	  response_stream << "250 OK\n";
	}
	else if(cmd=="DATA") {
	  isData_ = true;
	  response_stream << "354 Enter mail, end with \".\" on a line by itself\n";
	}
	else if(cmd=="QUIT") {
	  isQuit_ = true;
	  response_stream << "221 ukvpc.itandem.ru SMTP closing connection\n";
	}
	else
	  response_stream << "599 Unknown command\n";
      }
      
      boost::asio::async_write(socket_, response_,
	boost::bind(&session::handle_write, this,
		boost::asio::placeholders::error));
    }
    else
    {
      delete this;
    }
  }

  void handle_write(const boost::system::error_code& error)
  {
    if (!error)
    {
      if(isQuit_) {
	socket_.close();
	delete this;
      }
      else {
	if(isData_)
	  boost::asio::async_read_until(socket_, request_, "\r\n.\r\n",
					boost::bind(&session::handle_read, this,
						    boost::asio::placeholders::error));
	else
	  boost::asio::async_read_until(socket_, request_, "\r\n",
					boost::bind(&session::handle_read, this,
						    boost::asio::placeholders::error));
      }
    }
    else
    {
      delete this;
    }
  }

private:
  tcp::socket socket_;
  boost::asio::streambuf request_;
  boost::asio::streambuf response_;
  bool isData_;
  bool isQuit_;
};

class server
{
public:
  server(boost::asio::io_service& io_service, short port)
    : io_service_(io_service),
      acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
  {
    session* new_session = new session(io_service_);
    acceptor_.async_accept(new_session->socket(),
        boost::bind(&server::handle_accept, this, new_session,
          boost::asio::placeholders::error));
  }

  void handle_accept(session* new_session,
      const boost::system::error_code& error)
  {
    if (!error)
    {
      new_session->start();
      new_session = new session(io_service_);
      acceptor_.async_accept(new_session->socket(),
          boost::bind(&server::handle_accept, this, new_session,
            boost::asio::placeholders::error));
    }
    else
    {
      delete new_session;
    }
  }

private:
  boost::asio::io_service& io_service_;
  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: " << argv[0] << " <port>\n";
      return 1;
    }

    boost::asio::io_service io_service;

    using namespace std; // For atoi.
    server s(io_service, atoi(argv[1]));

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
