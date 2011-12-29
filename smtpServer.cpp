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


class notifier
{
public:
  notifier(boost::asio::io_service& io_service,
	   const std::string& host, unsigned short port,
	   const char* filename)
    : socket_(io_service),
      timer_(io_service)
  {
    filename_ = (char *)filename;
    socket_.async_connect(
		tcp::endpoint(boost::asio::ip::address::from_string(host),port),
		boost::bind(&notifier::handle_connect,
			      this, boost::asio::placeholders::error));
    
    std::cout << "notifier()\n";
  }

private:
  ~notifier()
  {
    std::cout << "~notifier()\n";
  }
  
  void handle_connect(const boost::system::error_code& error)
  {
    if (!error) {
      // Form the request. 
      std::ostream request_stream(&request_);
      const char body[] = "0022\"NEW-MSG\"1000L0#";
      
      request_stream << char(0x0A) << char(0xCE) << char(0x09)
		     << body << filename_ << "[]" << char(0x0D);

      timer_.expires_from_now(boost::posix_time::seconds(2));
      timer_.async_wait(boost::bind(&notifier::handle_timeout, this,
				   boost::asio::placeholders::error));

      // The connection was successful. Send the request.
      boost::asio::async_write(socket_, request_,
			       boost::bind(&notifier::handle_write, this,
					   boost::asio::placeholders::error));
    }
    else {
      std::ofstream logfile("notifier.err", std::ios::out | std::ios::app);
      if(logfile) {
      	logfile << boost::posix_time::second_clock::local_time()
      		<< " " << filename_
      		<< " [can't connect to server]\n";
      	logfile.close();
      }
      close();
    }
  }

  void handle_write(const boost::system::error_code& error)
  {
    if (!error) {
      // Read the response status line. The response_ streambuf will
      // automatically grow to accommodate the entire line. The growth may be
      // limited by passing a maximum size to the streambuf constructor.
      boost::asio::async_read_until(socket_, response_, "\n", 
				    boost::bind(&notifier::handle_read, this,
						boost::asio::placeholders::error));
    }
    else
      close();
  }

  void handle_read(const boost::system::error_code& error)
  {
    timer_.cancel();
    close();
  }


  void close()
  {
    if(socket_.is_open()) socket_.close();
    delete this;
  }

  void handle_timeout(const boost::system::error_code& error)
  {
    if(!error)
      socket_.close();
  }

  tcp::socket socket_;
  boost::asio::deadline_timer timer_;
  boost::asio::streambuf request_;
  boost::asio::streambuf response_;
  char *filename_;
};
  

class session
{
public:
  session(boost::asio::io_service& io_service,
	  const std::string& notice_host, unsigned short notice_port)
    : socket_(io_service),
      timer_(io_service),
      isData_(false), isQuit_(false),
      notice_host_(notice_host),
      notice_port_(notice_port)
  {
    std::cout << "session()\n";
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

    timer_.expires_from_now(boost::posix_time::seconds(10));
    timer_.async_wait(boost::bind(&session::handle_timeout, this,
    				  boost::asio::placeholders::error));
    
    std::ostream response_stream(&response_);

    response_stream << "220 ukvpc.itandem.ru SMTP is glad to see you!\n";
    boost::asio::async_write(socket_, response_,
		boost::bind(&session::handle_write, this,
			boost::asio::placeholders::error));
  }

  void close()
  {
    if(socket_.is_open()) socket_.close();
    timer_.cancel();
    delete this;
  }

private:
  ~session()
  {
    std::cout << "~session()\n";
    if(logfile_) {
      boost::posix_time::ptime endTime = boost::posix_time::microsec_clock::local_time();
      boost::posix_time::time_period tp(startTime_, endTime);
      if(logfile_) {
	logfile_ << endTime
		 << " -- Close connection"
		 << " [Duration " << tp.length() << "] --------\n\n";
	logfile_.close();
      }
    }
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
	notifier *notifier_ = new notifier(socket_.io_service(),
					   notice_host_, notice_port_, uniqname);
	if(!notifier_) assert(false);
	
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
      close();
  }

  void handle_write(const boost::system::error_code& error)
  {
    if (!error)
    {
      if(isQuit_)
	close();
      else
	boost::asio::async_read_until(socket_,
			request_, isData_ ? "\r\n.\r\n" : "\r\n",
		boost::bind(&session::handle_read, this,
			boost::asio::placeholders::error));
    }
    else
      close();
  }

  void handle_timeout(const boost::system::error_code& error)
  {
    if(!error) {
      if(logfile_) {
  	logfile_ << boost::posix_time::microsec_clock::local_time()
  		 << " Timed out.\n";
      }
      socket_.close();
    }
  }


private:
  tcp::socket socket_;
  boost::asio::deadline_timer timer_;
  boost::asio::streambuf request_;
  boost::asio::streambuf response_;
  bool isData_;
  bool isQuit_;
  std::ofstream logfile_;
  boost::posix_time::ptime startTime_;
  std::string notice_host_;
  unsigned short notice_port_;
};

class server
{
public:
  server(boost::asio::io_service& io_service,
	 short port,
	 const std::string& notice_host, unsigned short notice_port)
    : io_service_(io_service),
      acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
      notice_host_(notice_host),
      notice_port_(notice_port)
  {
    session* new_session = new session(io_service_, notice_host_, notice_port_);
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
      new_session = new session(io_service_, notice_host_, notice_port_);
      acceptor_.async_accept(new_session->socket(),
          boost::bind(&server::handle_accept, this, new_session,
            boost::asio::placeholders::error));
    }
    else
    {
      new_session->close(); // delete new_session; 
    }
  }

private:
  boost::asio::io_service& io_service_;
  tcp::acceptor acceptor_;
  std::string notice_host_;
  unsigned short notice_port_;
};

int main(int argc, char* argv[])
{
  try
  {
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
      ("port,p", po::value<int>(), "set port for listening")
      ("notify-address,a", po::value<std::string>()->default_value("127.0.0.1"), "set ip-address to notify")
      ("notify-port,n", po::value<int>()->default_value(45000), "set port to notify")
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
    server s(io_service,
	     vm["port"].as<int>(),
	     vm["notify-address"].as<std::string>(),
	     vm["notify-port"].as<int>());

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
