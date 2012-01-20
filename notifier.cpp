#include <cstdio>
#include <iostream>
#include <fstream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "notifier.h"

using boost::asio::ip::tcp;

notifier::notifier(boost::asio::io_service& io_service,
		   const std::string& host, unsigned short port,
		   const char* filename)
  : socket_(io_service),
    timer_(io_service)
{
  filename_ = (char *)filename;
  socket_.async_connect(tcp::endpoint(boost::asio::ip::address::from_string(host),
				      port),
			boost::bind(&notifier::handle_connect,
				    this, boost::asio::placeholders::error));
    
  std::cout << "notifier()\n";
}

notifier::~notifier()
{
  std::cout << "~notifier()\n";
}
  
void notifier::handle_connect(const boost::system::error_code& error)
{
  if (!error) {
    // The connection was successful. Send the request.
    std::ostream request_stream(&request_);
    const char body[] = "0021\"NEW-MSG\"1000L0#";
      
    request_stream << char(0x0A) << char(0xCE) << char(0x09)
		   << body << filename_ << "[|0999]" << char(0x0D);

    timer_.expires_from_now(boost::posix_time::seconds(2));
    timer_.async_wait(boost::bind(&notifier::handle_timeout, this,
				  boost::asio::placeholders::error));

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

void notifier::handle_write(const boost::system::error_code& error)
{
  if (!error) {
    boost::asio::async_read_until(socket_, response_, "\n", 
				  boost::bind(&notifier::handle_read, this,
					      boost::asio::placeholders::error));
  }
  else
    close();
}

void notifier::handle_read(const boost::system::error_code& error)
{
  timer_.cancel();
  close();
}


void notifier::close()
{
  if(socket_.is_open()) socket_.close();
  delete this;
}

void notifier::handle_timeout(const boost::system::error_code& error)
{
  if(!error)
    socket_.close();
}
