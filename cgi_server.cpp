#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <vector>
#include <sys/types.h>
#include <boost/asio.hpp>
#include <boost/algorithm/string/replace.hpp>

using boost::asio::ip::tcp;
using namespace std ;

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

struct client_info{
  char hostname[100] ;
  char port[10] ;
  char file[50] ;
} ;

vector<string> for_html_use ;  
client_info client[5] ;

class session 
    : public enable_shared_from_this<session>
{
  private:
      vector<string>command_list ;
      tcp::resolver resolver_ ;
      tcp::socket socket_ ;
      shared_ptr<tcp::socket> client_socket ;
      tcp::resolver::query query_ ;
      enum { max_size = 15000 };
      char data[max_size] ;
      string output_buffer ;
      string output_info ;
      fstream file_stream ;
      string html_id ;
      string shell_command ;
      int index = 0 ;
  public:
      session( boost::asio::io_context& io_context, tcp::resolver::query q, string file, int id, tcp::socket *c_socket )
      : resolver_(io_context), socket_(io_context), client_socket(c_socket), query_(move(q)) 
      { 
        depatch_id ( id ) ;
        file_stream.open( file, ios::in ) ;
        if ( !file_stream )
          cerr << "can't open file!\n" ;
        shell_command.clear() ;
        while ( getline( file_stream, shell_command ) ) {
          command_list.push_back(shell_command) ;
          shell_command.clear() ;
        } // while   
        file_stream.close() ; 
      } // session()

      void depatch_id( int id ) {
        if ( id == 0 )
          html_id = "s0" ;
        else if ( id == 1 ) 
          html_id = "s1" ;
        else if ( id == 2 ) 
          html_id = "s2" ;
        else if ( id == 3 ) 
          html_id = "s3" ;
        else if ( id == 4 ) 
          html_id = "s4" ;
      } // depatch_id()

      void start_connect(){
        auto self(shared_from_this()) ;
        auto cli_self(client_socket) ;
        resolver_.async_resolve( query_,
                    [this, self, cli_self](boost::system::error_code ec, tcp::resolver::iterator it)
                    {
                      if (!ec)
                        resolve_handler(it) ;
                    }) ;
      } // start_connect()
  
      void resolve_handler(tcp::resolver::iterator it ) {
        auto self(shared_from_this()) ;
        auto cli_self(client_socket) ;
        socket_.async_connect(*it,
                  [this, self, cli_self](boost::system::error_code ec)
                  {
                    if (!ec)
                      do_read_shell() ;
                  }) ;
      } // resolver_handler()
  
      void do_read_shell() {
        auto self(shared_from_this()) ;
        auto cli_self(client_socket) ;
        socket_.async_read_some( boost::asio::buffer(data, max_size),
                  [this, self, cli_self](boost::system::error_code ec, size_t length)
                  {
                    if (!ec) {
                      if ( check_percent() == true ) {
                        output_buffer = output_buffer + data ;  
                        memset( data, 0, max_size ) ;
                        output_shell( output_buffer ) ;
                        output_buffer.clear() ;
                        do_write_shell() ;
                      } // if 
                      else {
                        output_buffer = output_buffer + data ;
                        memset( data, 0, max_size ) ;
                        do_read_shell() ;   
                      } // else 
                    } // if 
                    else {  
                      cerr << "read eof:" << ec << "\n" ;
                    } // else   
                  });
      } // do_read_shell()

      void do_write_shell() {
        string command ;
        command = clean_str( command_list[index] ) ;
        command = command + "\r\n" ;
        output_command( command ) ;
        index = index + 1 ;
        auto self(shared_from_this()) ;
        auto cli_self(client_socket) ;
        boost::asio::async_write(socket_, boost::asio::buffer(command, command.length()),
                   [this, self, command, cli_self](boost::system::error_code ec, std::size_t /*length*/)
                     {
                        if (!ec) {
                          do_read_shell(); 
                          if (command == "exit\r\n")
                            cout << "exit!\n" ;
                        } // if 
                      });
      } // do_write_shell()

      string clean_str( string str ) {
        string new_str ;
        char temp[15000] ; 
        int j = 0 ;
        int i = 0 ;
        while ( str[i] != '\0') {
          if ( int(str[i]) >= 32 ) {
            temp[j] = str[i] ;
            j = j + 1 ;  
          } // if
          i = i + 1 ;
        } // while   
        temp[j] = '\0' ;
        new_str = temp ;
        return new_str ;
      } // clean_str

      bool check_percent() {
        for( int i = 0 ; i < max_size ; i++ ) {
          if ( data[i] == '%' )
            return true ; 
        } // for 

        return false ;
      } // check_persent()

      void output_command( string str ) {
        auto self(shared_from_this()) ;
        auto cli_self(client_socket) ;
        boost::algorithm::replace_all( str, "&", "&amp;" ) ;
        boost::algorithm::replace_all( str, "\"", "&quot;" ) ;
        boost::algorithm::replace_all( str, "\'", "&apos;" ) ;
        boost::algorithm::replace_all( str, "<", "&lt;" ) ;
        boost::algorithm::replace_all( str, ">", "&gt;" ) ;
        boost::algorithm::replace_all( str, "\r\n", "\n" ) ;
        boost::algorithm::replace_all( str, "\n", "<br>" ) ;
        output_info = "<script>document.getElementById('" + html_id + "').innerHTML += '<b>" + str + "</b>';</script>" ;
        boost::asio::async_write(*client_socket, boost::asio::buffer(output_info, output_info.length()),
           [this, self, cli_self](boost::system::error_code ec, std::size_t /*length*/)
           {
              if (ec)
                cerr << "ec:" << ec << "\n" ; 
           });
        output_info.clear() ;
      } // output_command()

      void output_shell( string str ) {
        auto self(shared_from_this()) ;
        auto cli_self(client_socket) ;
        boost::algorithm::replace_all( str, "&", "&amp;" ) ;
        boost::algorithm::replace_all( str, "\"", "&quot;" ) ;
        boost::algorithm::replace_all( str, "\'", "&apos;" ) ;
        boost::algorithm::replace_all( str, "<", "&lt;" ) ;
        boost::algorithm::replace_all( str, ">", "&gt;" ) ;
        boost::algorithm::replace_all( str, "\r\n", "\n" ) ;
        boost::algorithm::replace_all( str, "\n", "<br>" ) ;
        output_info = "<script>document.getElementById('" + html_id + "').innerHTML += '" + str + "';</script>" ;
        boost::asio::async_write(*client_socket, boost::asio::buffer(output_info, output_info.length()),
           [this, self, cli_self](boost::system::error_code ec, std::size_t /*length*/)
           {
              if (ec)
                cerr << "ec:" << ec << "\n" ; 
           });
        output_info.clear() ;
      } // output_shell()
} ; // session() 


class _client
  : public enable_shared_from_this<_client>
{
private:
  env_info env ;
  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
  char program_path[200] ;
  string return_html ;

public:
  _client(tcp::socket socket)
    : socket_(std::move(socket))
  {

  }

  void start() {
    do_read();
  } // start()

  string clean_str_forhtml( string str ) {
        string new_str ;
        char temp[15000] ; 
        int j = 0 ;
        int i = 0 ;
        while ( str[i] != '\0') {
          if ( int(str[i]) >= 32 ) {
            temp[j] = str[i] ;
            j = j + 1 ;  
          } // if
          i = i + 1 ;
        } // while   
        temp[j] = '\0' ;
        new_str = temp ;
        return new_str ;
  } // clean_str
  
  void do_read() {
    auto self(shared_from_this());
    cout << "still listen\n" ;
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec) {
            prepare_env() ;
            return_html = "HTTP/1.1 200 OK\r\n" ;
            do_write();
          } // if
        });
  } //do_read()

  void do_write() {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(return_html, return_html.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec) {
            if ( strcmp( program_path, "./console.cgi" ) == 0 ) { 
              console_cgi() ;
              do_read() ;
            } // if   
            else if ( strcmp( program_path, "./panel.cgi" ) == 0 ) {
              panel_cgi();
            } // else if   
          } // if 
          else 
            do_read() ;
        });
  } //do_write()

  void panel_cgi() {
    auto self(shared_from_this());
    fstream panel_stream ;
    string str ;
    string panel_output = "Content-type: text/html\r\n\r\n" ;
    vector<string> panel_html ;
    panel_html.clear() ;
    panel_stream.open( "panel.html", ios::in ) ;
    if ( !panel_stream )
      cerr << "can't open panel_stream!\n" ;
    else 
      cerr << "open panel.html success!\n" ;
    while ( getline(panel_stream, str) ) {
      str = clean_str_forhtml(str) ;
      panel_html.push_back(str) ;
    } // while   
    panel_stream.close() ;
    int size = panel_html.size() ;
    int i = 0 ;
    while ( i < size ) {
      panel_output = panel_output + panel_html[i] ;
      i = i + 1 ;
    } // while 
    boost::asio::async_write(socket_, boost::asio::buffer(panel_output, panel_output.length()),
           [this, self](boost::system::error_code ec, std::size_t /*length*/)
           {
              if (ec)
                cerr << "panel_ec:" << ec << "\n" ;
              else {
                cerr << "panel.html write finish!\n" ;
              } // else   
           });
  
  } // panel_cgi()

  void console_cgi() {
    for_html_use.clear() ;
    const char* del = "&" ;
    int i = 0 ;
    char** str_list = (char**)malloc(sizeof(char*)*15) ; // for the hx px fx ...
    char query_string[1024] ;
    char* temp ; 
    strcpy( query_string, env.QUERY_STRING.c_str() ) ;
    temp = strtok( query_string, del ) ;
    while ( temp != NULL ) {
      str_list[i] = temp ;
      temp = strtok( NULL, del ) ;
      i = i + 1 ;
    } // while 
  
    i = 0 ;
    for ( int j = 0 ; j < 5 ; j++ ) {
      set_client_info(str_list[i], j) ;
      set_client_info(str_list[i+1], j) ;
      set_client_info(str_list[i+2], j) ;
      i = i + 3 ;
    } // for 
    // finish set client
    free(str_list) ;
  
    to_output_ip_port() ;
    auto self(shared_from_this());
    string html_header = "Content-type: text/html\r\n\r\n" ;
    string html_content = html_header + R""""(
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8" />
                <title>NP Project 3 Sample Console</title>
                <link
                rel="stylesheet"
                href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
                integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
                crossorigin="anonymous"
                />
                <link
                href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
                rel="stylesheet"
                />
                <link
                rel="icon"
                type="image/png"
                href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
                />
                <style>
                * {
                    font-family: 'Source Code Pro', monospace;
                    font-size: 1rem !important;
                }
                body {
                    background-color: #212529;
                }
                pre {
                    color: #cccccc;
                }
                b {
                    color: #01b468;
                }
                </style>
            </head>
            <body>
                <table class="table table-dark table-bordered">
                <thead>
                    <tr>
                    <th scope="col">)"""" + for_html_use[0] + ":" + for_html_use[1] + "</th>" +
                    R"(<th scope="col">)" + for_html_use[2] + ":" + for_html_use[3] + "</th>" +
                    R"(<th scope="col">)" + for_html_use[4] + ":" + for_html_use[5] + "</th>" +
                    R"(<th scope="col">)" + for_html_use[6] + ":" + for_html_use[7] + "</th>" +
                    R"(<th scope="col">)" + for_html_use[8] + ":" + for_html_use[9] + "</th>" +
                    R"(</tr>
                </thead>
                <tbody>
                    <tr>
                    <td><pre id="s0" class="mb-0"></pre></td>
                    <td><pre id="s1" class="mb-0"></pre></td>
                    <td><pre id="s2" class="mb-0"></pre></td>
                    <td><pre id="s3" class="mb-0"></pre></td>
                    <td><pre id="s4" class="mb-0"></pre></td>
                    </tr>
                </tbody>
                </table>
            </body>
            </html>
        )";
    boost::asio::async_write(socket_, boost::asio::buffer(html_content, html_content.length()),
           [this, self](boost::system::error_code ec, std::size_t /*length*/)
           {
              if (ec)
                cerr << "ec:" << ec << "\n" ;
              else 
                cout << "console.html write finish!\n" ;
           });
    boost::asio::io_context ioc ;
    tcp::socket *socket_ptr = &socket_;
    for ( int i = 0 ; i < 5 ; i++ ) {
      if ( strcmp( client[i].hostname, "" ) != 0 && strcmp( client[i].port, "" ) != 0 && strcmp( client[i].file, "" ) != 0 ) {
        string h(client[i].hostname) ;
        string p(client[i].port) ;
        string f(client[i].file) ;
        f = "test_case/" + f ;
        tcp::resolver::query q(h, p) ;
        make_shared<session>( ioc, move(q), f, i, socket_ptr)->start_connect() ;
      } // if 
    } // for 
    
    ioc.run();
  } // console_cgi()

  void set_client_info( char* str, int index ) {
    int type = -1 ;
    if ( str[0] == 'h' )
      type = 0 ;
    else if ( str[0] == 'p' )
      type = 1 ;
    else if ( str[0] == 'f' )
      type = 2 ;
    else 
      return ;
    int i = 0 ;
    int j = 0 ;
    char temp_str[1024] ;
    memset( temp_str, 0, 1024) ;
    while ( str[i] != '=' )
      i++ ;
    i = i + 1 ; // skip the '='
    if ( str[i] != '\0' ) {
      while ( str[i] != '\0' ) {
        temp_str[j] = str[i] ;
        j = j + 1 ;
        i = i + 1 ;
      } // while 
      temp_str[j] = '\0';
      if ( type == 0 )
        strcpy( client[index].hostname, temp_str ) ;
      else if ( type == 1 )
        strcpy( client[index].port, temp_str ) ;
      else if ( type == 2 )
        strcpy( client[index].file, temp_str ) ;
    } // if 
    else {
      if ( type == 0 )
        strcpy( client[index].hostname, "" ) ;
      else if ( type == 1 )
        strcpy( client[index].port, "" ) ;
      else if ( type == 2 )
        strcpy( client[index].file, "" ) ;
    } // else 
    
  } // set_client_info

  void to_output_ip_port() {
    string temp = client[0].hostname ;
    for_html_use.push_back(temp) ;
    temp = client[0].port ;
    for_html_use.push_back(temp) ;
    temp = client[1].hostname ;
    for_html_use.push_back(temp) ;
    temp = client[1].port ;
    for_html_use.push_back(temp) ;
    temp = client[2].hostname ;
    for_html_use.push_back(temp) ;
    temp = client[2].port ;
    for_html_use.push_back(temp) ;
    temp = client[3].hostname ;
    for_html_use.push_back(temp) ;
    temp = client[3].port ;
    for_html_use.push_back(temp) ;
    temp = client[4].hostname ;
    for_html_use.push_back(temp) ;
    temp = client[4].port ;
    for_html_use.push_back(temp) ;
  } //to_output_ip_port()

  void prepare_env() {
    char request_method[10] ;
    char request_uri[1024] ;
    char query_string[1024] ;
    char server_protocol[100] ;
    char trash[100] ;
    char http_post[100] ;
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
}; // _client()

class server {
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<_client>(std::move(socket))->start();
          }

          do_accept();
        });
  } // do_accept()

  tcp::acceptor acceptor_;
}; // server()

int main(int argc, char* argv[])
{
  while (1) {
    try
    {
      boost::asio::io_context io_context ; 
      if (argc != 2)
      {
        std::cerr << "Usage: async_tcp_echo_server <port>\n";
        return 1;
      }

      server s(io_context, std::atoi(argv[1]));

      io_context.run();
    }
    catch (std::exception& e)
    {
      std::cerr << "Exception: " << e.what() << "\n";
    }
  } // while   

  return 0;
}