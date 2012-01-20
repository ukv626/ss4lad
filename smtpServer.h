#ifndef SMTPSERVER_H
#define SMTPSERVER_H

class session
{
public:
  session(boost::asio::io_service& io_service,
	  const std::string& notice_host, unsigned short notice_port);

  boost::asio::ip::tcp::socket& socket();

  void start();

  void close();

private:
  ~session();
  void handle_read(const boost::system::error_code& error);
  void handle_write(const boost::system::error_code& error);
  void handle_timeout(const boost::system::error_code& error);

  boost::asio::ip::tcp::socket socket_;
  boost::asio::deadline_timer timer_;
  boost::asio::streambuf request_;
  boost::asio::streambuf response_;
  std::ofstream logfile_;
  boost::posix_time::ptime startTime_;
  std::string notice_host_;
  unsigned short notice_port_;
  enum { WAIT, DATA, QUIT };
  unsigned char status_;
};

class server
{
public:
  server(boost::asio::io_service& io_service,
	 short port,
	 const std::string& notice_host, unsigned short notice_port);

  void handle_accept(session* new_session,
		     const boost::system::error_code& error);

private:
  boost::asio::io_service& io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::string notice_host_;
  unsigned short notice_port_;
};

#endif
