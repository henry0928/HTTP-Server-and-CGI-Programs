#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <sys/types.h>
#include <sys/wait.h>
#include <boost/asio.hpp>

using namespace std ;
using boost::asio::ip::tcp;

struct env_info{
  string REQUEST_METHOD ;
  string REQUEST_URI ;
  string QUERY_STRING ;
  string SERVER_PROTOCOL ;
  string HTTP_HOST ;
  string SERVER_ADDR ;
  string SERVER_PORT ;
  string REMOTE_ADDR ;
  string REMOTE_PORT ;
};

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {

  }

  void start()
  {
    do_read();
  }

private:
  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec) {
            pid_t pid = fork() ;
            if ( pid == -1 )
              cerr << "fork error()\n" ;
            else if ( pid == 0 ) { // child process
              prepare_env() ;
              set_env() ;
              //cerr << "query_string:" << getenv("QUERY_STRING") << "\n"  ;
              dup2( socket_.native_handle(), STDIN_FILENO ) ;
              dup2( socket_.native_handle(), STDOUT_FILENO ) ;
              dup2( socket_.native_handle(), STDERR_FILENO ) ;
              cout << "HTTP/1.1 200 OK\r\n" ;
              cout << "Content-Type: test/html\r\n" ;
              fflush(stdout) ;
              char* command[2] ;
              command[0] = program_path ;
              command[1] = NULL ;
              socket_.close() ;
              execvp( program_path, command ) ;
            } // else if 
            else { // parent process
              //waitpid(pid, NULL, 0) ;
              socket_.close() ;
            } // else   
            do_write(length);
          } // if 
        });
  } //do_read()

  void do_write(std::size_t length) {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec) {
            do_read();
          } // if 
        });
  } //do_write()

  void prepare_env() {
    char request_method[10] ;
    char request_uri[1024] ;
    char query_string[1024] ;
    char server_protocol[100] ;
    char trash[100] ;
    char http_post[100] ;
    //memset( query_string, 0, 1024 ) ;
    int i = 0 ; //skip the '/'
    program_path[0] = '.' ;
    int j = 0 ;
    sscanf( data_, "%s %s %s %s %s", request_method, request_uri
            , server_protocol, trash, http_post ) ;        
    while ( request_uri[i] != '?' && request_uri[i] != '\0' ) {
      program_path[i+1] = request_uri[i] ;
      i = i + 1 ;
    } // while 
    program_path[i+1] = '\0' ; // let program_path has null
    if ( request_uri[i] == '\0' )
      strcpy( query_string, "" ) ;
    else {
      i = i + 1 ; // skip the '?'
      while ( request_uri[i] != '\0' ) {
        query_string[j] = request_uri[i] ;
        j = j + 1 ;
        i = i + 1 ;
      } // while   
      query_string[j] = '\0' ; // let query_string has null 
    } // else
    env.REQUEST_METHOD = request_method ;
    env.REQUEST_URI = request_uri ;
    env.QUERY_STRING = query_string ;
    env.SERVER_PROTOCOL = server_protocol ;
    env.HTTP_HOST = http_post ;
    env.SERVER_ADDR = socket_.local_endpoint().address().to_string() ;
    env.SERVER_PORT = to_string(socket_.local_endpoint().port());
    env.REMOTE_ADDR = socket_.remote_endpoint().address().to_string() ;
    env.REMOTE_PORT = to_string(socket_.remote_endpoint().port()) ;
  } // prepare_env()

  void set_env() {
    setenv( "REQUEST_METHOD", env.REQUEST_METHOD.c_str(), 1 ) ;
    setenv( "REQUEST_URI", env.REQUEST_URI.c_str(), 1 ) ;
    setenv( "QUERY_STRING", env.QUERY_STRING.c_str(), 1 ) ;
    setenv( "SERVER_PROTOCOL", env.SERVER_PROTOCOL.c_str(), 1 ) ;
    setenv( "HTTP_HOST", env.HTTP_HOST.c_str(), 1 ) ;
    setenv( "SERVER_ADDR", env.SERVER_ADDR.c_str(), 1 ) ;
    setenv( "SERVER_PORT", env.SERVER_PORT.c_str(), 1 ) ;
    setenv( "REMOTE_ADDR", env.REMOTE_ADDR.c_str(), 1 ) ;
    setenv( "REMOTE_PORT", env.REMOTE_PORT.c_str(), 1 ) ;
  } // set_env()


  env_info env ;
  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
  char program_path[200] ;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}