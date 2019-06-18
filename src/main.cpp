#include <zmq.h>
#include <assert.h>
#include <string.h>
#include <string>
#include <iostream>
#include <regex>
#include <fstream>
#include <streambuf>
#include <algorithm>
#include <iterator>
#include <cerrno>
#include <filesystem>
#include <vector>
//#include "test.h"
#include <Python.h>
#include <boost/python.hpp>

std::string site_head(".");
std::string resp404("404 nono");
std::basic_regex<char> reg_METHOD_URI("(GET|POST|PUT|HEAD|DELETE|PATCH|OPTIONS)\\s+(\\S+)\\s+(HTTP)");
std::basic_regex<char> reg_FORMAT("Accept:\\s*(.*)");
std::basic_regex<char> reg_COMMAND("%(.*) (.*)%");
std::smatch MATCH;


struct pyInstance{
    boost::python::object pymain,global;
    pyInstance(){
        Py_Initialize();
        this->pymain = boost::python::import("__main__");
        this->global = this->pymain.attr("__dict__");
        char st[256];
        try
        {   
            //sprintf(st,"site_root = \"%s\"",site_head.c_str());
            //boost::python::exec(st,global,global);
            global["site_root"] = site_head; 
            boost::python::exec_file((site_head+"/functions.py").c_str(),global,global);
        }
        catch(boost::python::error_already_set const &)
        {
            PyErr_Print();
        }

        //boost::python::exec("import sys",global,global);
        //boost::python::exec("import os",global,global);
    }
    std::string exec(std::string code){
        //boost::python::object local = this->pymain.attr("__dict__");
        boost::python::exec(code.c_str(),global);
        return boost::python::extract<std::string>(global["ret"]);
    }
};
pyInstance mainpy;

std::vector<std::filesystem::path> getDirectories(const std::string& s)
{
    std::vector<std::filesystem::path> r;
    for(auto& p : std::filesystem::directory_iterator(site_head+s))
        if(p.status().type() == std::filesystem::file_type::directory)
            r.push_back(p.path().lexically_relative(site_head));
    return r;
}
bool endsWith (std::string const &fullString, std::string const &ending)
{
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

std::string getFileContents(std::string filename)
{
  std::ifstream in(site_head+filename, std::ios::in | std::ios::binary);
  if (in)
  {
    std::string contents,source;
    in.seekg(0, std::ios::end);
    source.reserve(in.tellg());
    in.seekg(0, std::ios::beg);
    std::copy((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>(), std::back_inserter(source));
    in.close();
    contents = source;
    if(endsWith(filename,".html")) {
            std::smatch tempMatch;
            auto comm_begin =
                    std::sregex_iterator(source.begin(), source.end(), reg_COMMAND);
            auto comm_end = std::sregex_iterator();
            int accum = 0;

            for (std::sregex_iterator i = comm_begin; i != comm_end; ++i) {
                tempMatch = *i;
                try {
                    if(tempMatch[1] == "include"){
                        std::string temp = getFileContents(tempMatch[2]);
                        contents.replace(accum+tempMatch.position(),tempMatch.length(),temp);
                        accum += temp.length()-tempMatch.length();
                    }
                    else if(tempMatch[1] == "pythonString"){
                        std::string output = tempMatch[0].str();
                        output.replace(0,tempMatch[1].length()+2,"");
                        output.replace(output.length()-1,1,"");
                        try
                        {   
                            output = mainpy.exec(output);
                        }
                        catch(boost::python::error_already_set const &)
                        {
                            PyErr_Print();
                        }
                        
                        contents.replace(accum+tempMatch.position(),tempMatch.length(),output);
                        accum += output.length()-tempMatch.length();
                    }
                    else if(tempMatch[1] == "python"){
                        
                        std::string output = tempMatch[0].str();
                        output.replace(0,tempMatch[1].length()+2,"");
                        output.replace(output.length()-1,1,"");
                        std::cout << output << std::endl;
                        try
                        {   
                            boost::python::exec(output.c_str(),mainpy.global);
                        }
                        catch(boost::python::error_already_set const &)
                        {
                            PyErr_Print();
                        }
                        
                        contents.replace(accum+tempMatch.position(),tempMatch.length(),"");
                        accum -= tempMatch.length();
                    }
                }
                catch(...) {
                    std::cout << "Failed processing command on " << filename << " " << MATCH[0].str() << std::endl;
                }
            }


    }
    return(contents);
  }
  std::cout << "Failed opening: " << site_head + filename << std::endl;
  throw(errno);
}


struct httprequest {

    enum Method {
        GET=0,
        POST,
        PUT,
        HEAD,
        DELETE,
        PATCH,
        OPTIONS};
    Method method;
    std::string uri;
    std::string format;
    std::string fullRequest;
    static std::string urlDecode(std::string SRC) {
        std::string ret;
        char ch;
        int i, ii;
        for (i=0; i<SRC.length(); i++) {
            if (int(SRC[i])==37) {
                sscanf(SRC.substr(i+1,2).c_str(), "%x", &ii);
                ch=static_cast<char>(ii);
                ret+=ch;
                i=i+2;
            } else {
                ret+=SRC[i];
            }
        }
        return (ret);
    }
    static httprequest* parse(std::string fullRequest){
        httprequest *ret = new httprequest;
        ret->fullRequest = fullRequest;
        if(std::regex_search(fullRequest, MATCH, reg_METHOD_URI)){
            auto meth = MATCH[1].str();
            ret->uri = urlDecode(MATCH[2].str());
            if     (meth == "GET")      ret->method = GET;
            else if(meth == "POST")     ret->method = POST;
            else if(meth == "PUT")      ret->method = PUT;
            else if(meth == "HEAD")     ret->method = HEAD;
            else if(meth == "DELETE")   ret->method = DELETE;
            else if(meth == "PATCH")    ret->method = PATCH;
            else if(meth == "OPTIONS")  ret->method = OPTIONS;
            else {
                delete ret;
                return nullptr;
            }
        } else {
            delete ret;
            return nullptr;
        }
        if(std::regex_search(fullRequest, MATCH, reg_FORMAT)){
            if(MATCH.size() == 2)
                ret->format = MATCH[1].str();
            else {
                delete ret;
                return nullptr;}
        } else {
            delete ret;
            return nullptr;}
        if(ret->uri.back() == '/'){
            ret->uri += "index.html";
        }
        return ret;
    }
};
struct httpresponse{
    enum Code {
        OK=200,
        CREATED=201,
        ACCEPTED=202,
        FORBIDDEN=403,
        NOTFOUND=404};
    Code responseCode;
    std::string contentType;
    std::string content;
    std::string get(){
        std::string ret("HTTP/1.0 ");
        ret += responseCode;
        ret += "\r\nContent-Type: ";
        ret += contentType;
        ret += "\r\n\r\n";
        ret += content;
        return ret;
    }
};
int main(){
    void *ctx = zmq_ctx_new ();
    assert (ctx);
    /* Create ZMQ_STREAM socket */
    void *socket = zmq_socket (ctx, ZMQ_STREAM);
    assert (socket);
    int rc = zmq_bind (socket, "tcp://*:80");
    assert (rc == 0);
    //eval();
    // Retrieve the main module.
    
    while (1) {
        zmq_msg_t id,req;
        zmq_msg_init(&id);
        zmq_msg_init(&req);
        /* Get HTTP request; ID frame and then request */
        int stat = zmq_msg_recv (&id,socket,0);
        assert (stat > 0);

        stat = zmq_msg_recv (&req, socket, 0);
        assert (stat >= 0);

        //std::cout << request << std::endl;
        if(stat == 0){
            //std::cout << "Connection opened" << std::endl;
            zmq_msg_close(&id);
            zmq_msg_close(&req);
            continue;
        }
        auto request = httprequest::parse(std::string((char*)zmq_msg_data(&req),zmq_msg_size(&req)));
        if(request){
            std::cout << request->uri << std::endl;
            //std::cout << request->format << std::endl;
        }
        //std::cout << request->format << std::endl;
        /* Prepares the response */
        httpresponse resp;
        resp.responseCode = httpresponse::OK;

        if(endsWith(request->uri,".png")){
            resp.contentType = "image/png";
        }
        else{
            resp.contentType = "text/html";
        }

        try{
            resp.content += getFileContents(request->uri);
        }
        catch(...){
            resp.content += resp404;
            resp.responseCode = httpresponse::NOTFOUND;
            //std::cout << std::strerror(errno) << std::endl;
        }

        std::string http_response = resp.get();
        //std::cout << http_response.length() << std::endl;

        /* Sends the ID frame followed by the response */
        zmq_send(socket,zmq_msg_data(&id),zmq_msg_size(&id),ZMQ_SNDMORE);
        zmq_send(socket,http_response.c_str(),http_response.length(),0);
        zmq_msg_send (&id,socket,ZMQ_SNDMORE);
        zmq_send (socket, 0, 0, 0);
        zmq_msg_close(&id);
        zmq_msg_close(&req);
    }
    zmq_close (socket);
    zmq_ctx_destroy (ctx);
}