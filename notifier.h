#ifndef NOTIFIER_H
#define NOTIFIER_H

class notifier
{
public:
  notifier(boost::asio::io_service& io_service,
	   const std::string& host, unsigned short port,
	   const char* filename);

private:
  void handle_connect(const boost::system::error_code& error);
  void handle_write(const boost::system::error_code& error);
  void handle_read(const boost::system::error_code& error);
  void close();
  void handle_timeout(const boost::system::error_code& error);
  
  boost::asio::ip::tcp::socket socket_;
  boost::asio::deadline_timer timer_;
  boost::asio::streambuf request_;
  boost::asio::streambuf response_;
  char *filename_;
};

#endif
