#include "wiscAFS_client.hh"
#include "cache/ClientCache.h"
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientReader;
using grpc::ClientWriter;
#include <dirent.h>
#include <fuse.h>



#ifdef __cplusplus
extern "C" {
#endif
int wiscAFSClient::OpenFile(const std::string& filename, const int flags) {
   //ClientCacheValue *ccv1 = diskCache.getCacheValue(filename);
   //if(ccv1 == nullptr){
   // Data we are sending to the server. ##ASSUMING FILENAMES include path
      
   RPCRequest request;
   request.set_filename(filename);
   request.set_flags(flags);

   // Container for the data we expect from the server.
   RPCResponse reply;

   // Context for the client. It could be used to convey extra information to
   // the server and/or tweak certain RPC behaviors.
   ClientContext context;
   int flog = open("/users/vramadas/client.log", O_CREAT|O_RDWR|O_TRUNC, 0777);

   // The actual RPC.
   write(flog, "WiscAFSClint:: Calling OpenFile Stub\n", strlen("WiscAFSClint:: Calling OpenFile Stub\n"));

   // The actual RPC.
   Status status = stub_->OpenFile(&context, request, &reply);
   // Act upon its status.
   if (status.ok()) {
       write(flog, "Received status ok\n", strlen("Received status ok\n"));
       std::cout << "Client: Reply status " << reply.status() << std::endl;
       std::string local_path = (client_path + std::to_string(reply.fileinfo().st_ino()) + ".tmp").c_str();
       int fileDescriptor = open(local_path.c_str(),  flags, 0644);
       if (fileDescriptor < 0) {
           std::cout << "Cannot open temp file " << local_path << std::endl;
       }

       if (fileDescriptor != -1) {
           std::cout << "Client: OPEN: Reply data received at client = " << reply.data() << std::endl;
           ssize_t writeResult = write(fileDescriptor, reply.data().c_str(), reply.data().size());
           printf("Client: writeResult = %ld\n", writeResult);
           //SUCCESS
           printf("Client: Printing fileatts = %d, %ld, %ld\n", reply.fileinfo().st_size(),reply.fileinfo().st_atim(),reply.fileinfo().st_mtim());
           CacheFileInfo fileatts;
           fileatts.setFileInfo(&reply.fileinfo());
           ClientCacheValue ccv(fileatts, false, fileDescriptor);
           diskCache.addCacheValue(filename, ccv);
           } else {
            write(flog, "Received status not ok\n", strlen("Received status ok\n"));
            return fileDescriptor;
            }
     }
   else {
       write(flog, "Received status not ok\n", strlen("Received status ok\n"));
       return -1;
   }
   //}
   /*else{
       //ALREADY IN CACHE
       int fd = ccv1->fileDescriptor;
       return fd;
   }*/
}

int wiscAFSClient::CloseFile(const std::string& filename) {
    // Data we are sending to the server.
    ClientCacheValue *ccv1 = diskCache.getCacheValue(filename);
    int fd = open("/users/vramadas/test.log", O_CREAT|O_RDWR|O_APPEND, 0777);
    if(ccv1 == nullptr){
        errno=ENOENT;
    }
//    else if(!ccv1->isDirty){
//        diskCache.deleteCacheValue(filename);
//        return 0;
//    }
    else{
    write(fd, "Inside wiscAFSClient::CloseFile\n" , strlen("Inside wiscAFSClient::CloseFile\n")); 
        RPCRequest request;
        request.set_filename(filename);
        request.set_filedescriptor(ccv1->fileDescriptor);
        //request.set_data(data);
        // Container for the data we expect from the server.
        RPCResponse reply;

        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // The actual RPC.
        Status status = stub_->CloseFile(&context, request, &reply);

        if (status.ok()) {
            diskCache.deleteCacheValue(filename);
            return 0;
        }
        else{
            return -status.error_code();
        }
    }
}

int wiscAFSClient::ReadFile(const std::string& filename) {
    ClientCacheValue *ccv1 = diskCache.getCacheValue(filename);
    if(ccv1 == nullptr){
        errno=ENOENT;
    }
    else{
        return ccv1->fileDescriptor;
    }
}

//Returning either fD or error
int wiscAFSClient::WriteFile(const std::string& filename){
    ClientCacheValue *ccv1 = diskCache.getCacheValue(filename);
    if(ccv1 == nullptr){
        errno=ENOENT;
    }
    else{
        if(!ccv1->isDirty){
            ccv1->isDirty = true;
            diskCache.updateCacheValue(filename, *ccv1);
        }
        return ccv1->fileDescriptor;
    }
}

RPCResponse wiscAFSClient::DeleteFile(const std::string& filename) {
   RPCRequest request;
   request.set_filename(filename);
   // Container for the data we expect from the server.
   RPCResponse reply;

   // Context for the client. It could be used to convey extra information to
   // the server and/or tweak certain RPC behaviors.
   ClientContext context;

   // The actual RPC.
   Status status = stub_->DeleteFile(&context, request, &reply);

   ClientCacheValue *ccv1 = diskCache.getCacheValue(filename);
   if(ccv1 == nullptr){
       errno=ENOENT;
   }
   else if (status.ok()) {
       diskCache.deleteCacheValue(filename);
   }
   return reply;

}

RPCResponse wiscAFSClient::CreateDir(const std::string& dirname, const int mode) {
   // Data we are sending to the server.
   RPCRequest request;
   request.set_filename(dirname);
   std::cout<<mode;
   request.set_mode(mode);

   // Container for the data we expect from the server.
   RPCResponse reply;

   // Context for the client. It could be used to convey extra information to
   // the server and/or tweak certain RPC behaviors.
   ClientContext context;
    int fd = open("/users/vramadas/test.log", O_CREAT|O_RDWR|O_APPEND, 0777);
   // The actual RPC.
   Status status = stub_->CreateDir(&context, request, &reply);
   write(fd, "CreateDir Passed\n", strlen("CreateDir Passed\n"));
   std::cout << status.ok();
   std::cout<<"This is the start" << std::endl;
   // Act upon its status.
   if (status.ok()) {
       std::cout << " I am open" << reply.data(); 
       return reply;
   } else {

       std::cout << status.error_code() << ": " << status.error_message()
           << std::endl;
       reply.set_status(-status.error_code());
       return reply;
   }
}

RPCResponse wiscAFSClient::OpenDir(const std::string& dirname, const int mode) {
    RPCRequest request;
    request.set_filename(dirname);

    // Container for the data we expect from the server.
    RPCResponse reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->OpenDir(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
        return reply;
    } else {
        std::cout << status.error_code() << ": " << status.error_message()
            << std::endl;
        reply.set_status(-1);
        return reply;
    }
}

int wiscAFSClient::ReadDir(const std::string& p, void *buf, fuse_fill_dir_t filler) {
    RPCRequest request;
    std::cout << "This is READDIR" << std::endl;
    std::cout << p << std::endl;
    request.set_filename(p);
    RPCDirReply reply;
    dirent de;
    reply.set_error(-1);
    ClientContext context;
    std::unique_ptr<ClientReader<RPCDirReply>> reader(stub_->ReadDir(&context, request));
    while(reader->Read(&reply)){
        struct stat st;
        memset(&st, 0, sizeof(st));

        de.d_ino = reply.dino();
        strcpy(de.d_name, reply.dname().c_str());
        de.d_type = reply.dtype();

        st.st_ino = de.d_ino;
        st.st_mode = de.d_type << 12;
        std::cout << de.d_ino << " ANBC " << de.d_type << " bhadbj " << st.st_ino << std::endl;
        if (filler(buf, de.d_name, &st, 0))
            break;
    }
    Status status = reader->Finish();
    return -status.error_code();
}

RPCResponse wiscAFSClient::RemoveDir (const std::string& dirname) {
    // Data we are sending to the server.
    RPCRequest request;
    request.set_filename(dirname);
    request.set_path(dirname);

    // Container for the data we expect from the server.
    RPCResponse reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->RemoveDir(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
        return reply;
    } else {
        std::cout << status.error_code() << ": " << status.error_message()
            << std::endl;
        reply.set_status(status.error_code());
        return reply;
    }
}

RPCResponse wiscAFSClient::GetAttr(const std::string& filename) {
   // Data we are sending to the server.
   RPCRequest request;
   request.set_filename(filename);

   // Container for the data we expect from the server.
   RPCResponse reply;

   // Context for the client. It could be used to convey extra information to
   // the server and/or tweak certain RPC behaviors.
   ClientContext context;

   // The actual RPC.
   Status status = stub_->GetAttr(&context, request, &reply);

   // Act upon its status.
   if (status.ok()) {
       return reply;
   } else {
       reply.set_status(-status.error_code());
       return reply;
   }
 
}

RPCResponse wiscAFSClient::Statfs(const std::string& filename) {
   RPCRequest request;
   request.set_filename(filename);

   RPCResponse reply;

   ClientContext context;

   Status status = stub_->StatFS(&context, request, &reply);

   if (status.ok()) {
      return reply;
   } else {
      reply.set_status(-status.error_code());
      return reply;
   }
}
#ifdef __cplusplus
}
#endif
