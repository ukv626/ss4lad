//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2011 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdio>
#include <iostream>
#include <fstream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using boost::asio::ip::tcp;
namespace po = boost::program_options;

class session
{
public:
  session(boost::asio::io_service& io_service)
    : socket_(io_service), isData_(false), isQuit_(false)
  {
  }

  ~session()
  {
    if(logfile_) {
      boost::posix_time::ptime endTime = boost::posix_time::microsec_clock::local_time();
      boost::posix_time::time_period tp(startTime_, endTime); 
      logfile_ << endTime
	       << " -- Close connection"
	       << " [Duration " << tp.length() << "] --------\n\n";
      logfile_.close();
    }
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void start()
  {
    startTime_ = boost::posix_time::microsec_clock::local_time();
    std::string s = socket_.remote_endpoint().address().to_string();
    s.append(".log");
    logfile_.open(s.c_str(), std::ios::out | std::ios::binary | std::ios::app);
    if(logfile_) {
      logfile_ << startTime_
	       <<  " -- New connection -------------------------------------\n";
      logfile_.flush();
    }
    
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
	char *uniqname = strdup("spool/XXXXXX.eml");
	mkstemps(uniqname, 4);
	
	std::ofstream outfile(uniqname, std::ios::out | std::ios::binary);
	if(outfile) {
	  if (response_.size() > 0)
	    outfile << &request_;
	  outfile.close();
	}

	// send notice like ADM-CID
	std::string cmd("./sendNotice localhost 45000 ");
	cmd += uniqname;
	system(cmd.c_str());

	isData_ = false;
      }
      else {
	std::string cmd;
	request_stream >> cmd;
	std::getline(request_stream, message);
	if(logfile_) {
	  logfile_ << boost::posix_time::microsec_clock::local_time()
		   << " " << cmd << message << std::endl;
	  logfile_.flush();
	}
	
	if("HELO"==cmd ||
	   "EHLO"==cmd ||
	   "MAIL"==cmd ||
	   "RCPT"==cmd ||
	   "NOOP"==cmd) {
	  response_stream << "250 OK\n";
	}
	else if("DATA"==cmd) {
	  isData_ = true;
	  response_stream << "354 Enter mail, end with \".\" on a line by itself\n";
	}
	else if("QUIT"==cmd) {
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
	boost::asio::async_read_until(socket_,
				      request_,
				      isData_ ? "\r\n.\r\n" : "\r\n",
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
  std::ofstream logfile_;
  boost::posix_time::ptime startTime_;
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
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
      ("port,p", po::value<int>(), "set port for listening")
      ("help", "show this information")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help") || !vm.count("port")) {
      std::cout << desc << "\n";
      return 1;
    }

    boost::asio::io_service io_service;
    server s(io_service, vm["port"].as<int>());

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
