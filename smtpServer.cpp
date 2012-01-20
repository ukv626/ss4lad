//
// ss4lad
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//

#include <cstdio>
#include <iostream>
#include <fstream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>

#include "notifier.h"
#include "smtpServer.h"

using boost::asio::ip::tcp;
namespace po = boost::program_options;


session::session(boost::asio::io_service& io_service,
		 const std::string& notice_host, unsigned short notice_port)
  : socket_(io_service),
    timer_(io_service),
    notice_host_(notice_host),
    notice_port_(notice_port),
    status_(WAIT)
{
  std::cout << "session()\n";
}

tcp::socket& session::socket()
{
  return socket_;
}

void session::start()
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

void session::close()
{
  if(socket_.is_open()) socket_.close();
  timer_.cancel();
  delete this;
}

session::~session()
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


void session::handle_read(const boost::system::error_code& error)
{
  if (!error)
    {
      std::istream request_stream(&request_);
      std::ostream response_stream(&response_);
      std::string message;

      if(DATA==status_) {
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
					   notice_host_, notice_port_, uniqname + 6);
	if(!notifier_) assert(false);
	
	status_ = WAIT;
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
	
	if("HELO" == cmd ||
	   "EHLO" == cmd ||
	   "MAIL" == cmd ||
	   "RCPT" == cmd ||
	   "NOOP" == cmd) {
	  response_stream << "250 OK\n";
	}
	else if("DATA" == cmd) {
	  status_ = DATA;
	  response_stream << "354 Enter mail, end with \".\" on a line by itself\n";
	}
	else if("QUIT" == cmd) {
	  status_ = QUIT;
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

void session::handle_write(const boost::system::error_code& error)
{
  if (!error)
    {
      if(QUIT == status_)
	close();
      else
	boost::asio::async_read_until(socket_,
				      request_, DATA==status_ ? "\r\n.\r\n" : "\r\n",
				      boost::bind(&session::handle_read, this,
						  boost::asio::placeholders::error));
    }
  else
    close();
}

void session::handle_timeout(const boost::system::error_code& error)
{
  if(!error) {
    if(logfile_) {
      logfile_ << boost::posix_time::microsec_clock::local_time()
	       << " Timed out.\n";
    }
    socket_.close();
  }
}


// -----------------
server::server(boost::asio::io_service& io_service,
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

void server::handle_accept(session* new_session,
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


// -------------------
void signals_handler(int sig)
{
  syslog(LOG_INFO, "Daemon stopped.");
  exit(EXIT_SUCCESS);
}

void daemonize()
{
  unsigned int i, fd0, fd1, fd2;
  pid_t pid;
  struct rlimit r1;

  // сбросить маску режима создания файла
  umask(0);

  if(getrlimit(RLIMIT_NOFILE, &r1) < 0)
    std::cerr << "Error: Невозможно получить максимальный номер дескриптора\n";

  // стать лидером сессии чтобы утратить управляющий терминал
  if((pid = fork()) < 0)
    std::cerr << "Error: Ошибка вызова функции fork\n";
  else if(pid != 0) /* родительский процесс*/
    exit(EXIT_SUCCESS);

  setsid();
  
  // обратываем сигналы
  struct sigaction sa;
  sa.sa_handler = signals_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if(sigaction(SIGHUP, &sa, NULL) < 0)
    std::cerr << "Error: невозможно игнорировать сигнал SIGHUP\n";

  // обеспечить невозможность обретения управляющего терминала в будущем (System V)
  // if((pid = fork()) < 0)
  //   std::cerr << "Error: Ошибка вызова функции fork\n";
  // else if(pid != 0) /* родительский процесс*/
  //   exit(EXIT_SUCCESS);

  // назначить корневой каталог текущим, чтобы впоследствии можно было
  // отмонтировать файловую систему 
  // if(chdir("/") < 0)
  //   std::cerr << "Error: невозможно сделать текущим рабочим каталогом /\n";

  // закрыть все открытые файловые дескрипторы
  if(r1.rlim_max == RLIM_INFINITY)
    r1.rlim_max = 1024;
  for(i = 0; i < r1.rlim_max; ++i)
    close(i);

  // присоеденить файловые дескрипторы к /dev/null
  fd0 = open("/dev/null", O_RDWR);
  fd1 = dup(0);
  fd2 = dup(0);

  if(fd0 != 0 || fd1 != 1 || fd2 != 2) {
    syslog(LOG_ERR, "Ошибочные файловые дескрипторы %d %d %d",fd0, fd1, fd2);
    exit(EXIT_FAILURE);
  }
}

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

    daemonize();

    boost::asio::io_service io_service;
    server s(io_service,
	     vm["port"].as<int>(),
	     vm["notify-address"].as<std::string>(),
	     vm["notify-port"].as<int>());

    syslog(LOG_INFO, "Daemon started.");
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return EXIT_SUCCESS;
}
