#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <netdb.h>      // for getaddrinfo, freeaddrinfo
#include <unistd.h>     // for close
#include <sys/socket.h> // for socket, connect, send, recv
#include <sys/types.h>  // for addrinfo

// Function to parse URL
bool parse_url(const std::string &url, std::string &host, std::string &path)
{
    size_t protocol_end = url.find("://");
    size_t host_start = (protocol_end == std::string::npos) ? 0 : protocol_end + 3; // 3 is the length of "://"
    size_t path_start = url.find('/', host_start);
    if (path_start == std::string::npos)
    {
        host = url.substr(host_start);
        path = "/";
    }
    else
    {
        host = url.substr(host_start, path_start - host_start);
        path = url.substr(path_start);
    }
    return !host.empty();
}

// Function to connect to the host
int connect_to_server(const std::string &host, int port)
{
    struct addrinfo hints, *res, *p;
    int sockfd;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0)
    {
        std::cerr << "Error in getaddrinfo\n";
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) // Create a socket and check for errors
        {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        std::cerr << "Failed to connect\n";
        return -2;
    }

    freeaddrinfo(res);
    return sockfd;
}

int hex_to_int(const std::string &hex_str)
{
    int result = 0;
    for (size_t i = 0; i < hex_str.length(); ++i)
    {
        char c = hex_str[i];
        if (c >= '0' && c <= '9')
        {
            result = result * 16 + (c - '0');
        }
        else if (c >= 'a' && c <= 'f')
        {
            result = result * 16 + (c - 'a' + 10);
        }
        else if (c >= 'A' && c <= 'F')
        {
            result = result * 16 + (c - 'A' + 10);
        }
        else
        {
            break;
        }
    }
    return result;
}

// Function to send GET request and receive response
void get_request(const std::string &host, const std::string &path, const std::string &output_file)
{
    int sockfd = connect_to_server(host, 80); // HTTP port is 80
    if (sockfd < 0)
    {
        std::cerr << "Connection failed\n";
        return;
    }

    std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: keep-alive\r\n\r\n";
    send(sockfd, request.c_str(), request.size(), 0);

    std::ofstream file(output_file, std::ios::binary);
    std::stringstream response_stream;
    char buffer[1024]; // increasing the size of the buffer may help (8192)
    int bytes_received;
    bool header_ended = false;
    bool chunked = false;
    int content_length = 0;
    int received_length = 0;

    while ((bytes_received = recv(sockfd, buffer, 1024, 0)) > 0)
    {
        if (!header_ended)
        {
            response_stream.write(buffer, bytes_received);
            std::string response = response_stream.str();
            size_t header_end_pos = response.find("\r\n\r\n");
            if (header_end_pos != std::string::npos)
            {
                header_ended = true;
                std::string headers = response.substr(0, header_end_pos);
                if (headers.find("Transfer-Encoding: chunked") != std::string::npos)
                {
                    chunked = true;
                }
                size_t cl_pos = headers.find("Content-Length: ");
                if (cl_pos != std::string::npos)
                {
                    content_length = std::stoi(headers.substr(cl_pos + 16));
                }
                file.write(response.c_str() + header_end_pos + 4, response.length() - header_end_pos - 4); // Write the first part of the response after the headers to the file
                received_length += response.length() - header_end_pos - 4;                                 // Update the received length
                response_stream.str("");
                response_stream.clear();
            }
        }
        else
        {
            if (chunked)
            {
                std::string chunk_size_str;
                std::getline(response_stream, chunk_size_str);
                int chunk_size = hex_to_int(chunk_size_str); // We have to use hex_to_int instead of std::stoi because the chunk size is in hexadecimal.
                if (chunk_size == 0)
                {
                    break;
                }
                char chunk_data[chunk_size];
                int bytes_read = 0;
                while (bytes_read < chunk_size)
                {
                    response_stream.read(chunk_data + bytes_read, chunk_size - bytes_read);
                    bytes_read += response_stream.gcount(); // Update the number of bytes read, gcount() returns the number of characters extracted by the last unformatted input operation.
                }
                file.write(chunk_data, chunk_size);
                received_length += chunk_size;
            }
            else
            {
                file.write(buffer, bytes_received);
                received_length += bytes_received;
            }
        }
        if (content_length > 0 && received_length >= content_length)
        {
            break;
        }
    }

    file.close();
    close(sockfd);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <URL> <output_file>\n";
        return 1;
    }

    std::string url = argv[1];
    std::string output_file = argv[2];
    std::string host, path;

    if (!parse_url(url, host, path))
    {
        std::cerr << "Invalid URL\n";
        return 2;
    }

    get_request(host, path, output_file);
    return 0;
}
