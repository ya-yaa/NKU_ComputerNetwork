#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
# define PORT 8080  // 端口号
# define buf_size 1024  // 缓冲区大小
# define MaxClient 5  // 最大连接数

# include <iostream>
// Socket库
# include <WinSock2.h>
# pragma comment(lib,"ws2_32.lib")

using namespace std;

SOCKET clientSockets[MaxClient];  // 客户端socket数组
bool clientflag[MaxClient];  // 记录客户端状态
int cur_connect = 0;  // 当前客户端数量

// 线程函数
DWORD WINAPI thread_func(LPVOID lpThreadParameter) {
	// 客户端编号
	int client_num = (int)lpThreadParameter;
	// 接受、发送缓冲区
	char rbuffer[buf_size];
	char sbuffer[buf_size];

	// 开始通讯	
	while (true) {
		// 接收
		/*int recv(
		SOCKET s,  // 客户端socket
		char FAR* buf,  // 存储接收数据的buf
		int len,  // 接受的长度
		int flags  // 影响函数行为的标志位
	    );*/
		int recvflag = recv(clientSockets[client_num], rbuffer, sizeof(rbuffer), 0);
		
		if (recvflag > 0) {  // 接收成功
			// 记录当前时间
			SYSTEMTIME st;
			GetLocalTime(&st);
			//  格式化时间字符串
			char timeStr[100];
			sprintf(timeStr, "[%04d-%02d-%02d %02d:%02d:%02d]",
				st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond);
			
			// 按格式输出信息
			cout << "Client " << clientSockets[client_num] << ": " << rbuffer << "----" << timeStr << endl;

			// 要将收到的信息广播给所有客户端
			// 格式化发送信息
			sprintf_s(sbuffer, sizeof(sbuffer), "%s -- %d:\n%s", timeStr, clientSockets[client_num], rbuffer);
			// 发送
			for (int i = 0; i < MaxClient; i++) {
				// 如果当前客户端存在
				if(clientflag[i]){
					send(clientSockets[i], sbuffer, strlen(sbuffer), 0);
				}
			}   	
		}
		else {  // 接收失败，通讯结束
			// 客户端主动关闭连接
			if (WSAGetLastError() == 10054)
			{
				// 记录当前时间
				SYSTEMTIME st;
				GetLocalTime(&st);
				//  格式化时间字符串
				char timeStr[100];
				sprintf(timeStr, "[%04d-%02d-%02d %02d:%02d:%02d]",
					st.wYear, st.wMonth, st.wDay,
					st.wHour, st.wMinute, st.wSecond);

				cout << "Client " << clientSockets[client_num] << " exit! ---- " << timeStr << endl;
				// 关闭客户端，更新连接情况
				closesocket(clientSockets[client_num]);
				cur_connect--;
				clientflag[client_num] = 0;
				cout << "Current connect num: " << cur_connect << endl;

				return 0;
			}
			else  // 其他错误
			{
				cout << "fail to receive!!" << "errcode: " << GetLastError() << endl;
			    break;
			}
		}
	}
	return 0;
}

int main() {
	// 开启网络权限
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	
	// 创建socket套接字
	/*SOCKET socket(
		int af,  // 协议地址族
		int type,  // 套接字类型
		int protocol  // 协议
	);*/
	// 监听socket,接受客户端申请。IPv4地址簇，流式套接字
	SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == -1) { 
		cout << "fail to create listen_socket!!\n" << "errcode: " << GetLastError() << endl;

		system("pause");
		return -1;
	}
	else { cout << "success create listen_socket!\n"; }

	// socket绑定端口
	// IPv4 套接字地址
	//typedef struct sockaddr_in {
	//	ADDRESS_FAMILY sin_family;  // 协议地址族
	//	USHORT sin_port;  // 端口号
	//	IN_ADDR sin_addr;  // IP地址(选项，只接受哪个网卡的数据)
	//	CHAR sin_zero[8];  // 保留字节
	//} 
	struct sockaddr_in server = { 0 };
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);  // htons转成大端序
	server.sin_addr.s_addr = htonl(INADDR_ANY);  //htonl转成大端序，INADDR_ANY全零，接受所有
	// 绑定
	/*bind(
		SOCKET s,
	    const struct sockaddr FAR * name,
		int namelen
	);*/
	int bindflag = bind(listen_socket, (struct sockaddr*)&server, sizeof(server));
	if (bindflag == -1) {
		cout << "fail to bind listen_socket!!\n" << "errcode: " << GetLastError() << endl;

		system("pause");
		return -1;
	}
	else { cout << "success bind listen_socket!\n"; }


	// 开启socket监听属性
	/*listen(
		SOCKET s,  
		int backlog  // 连接队列的最大长度
	);*/
	int listen_flag = listen(listen_socket, 10);
	if (listen_flag == -1) {
		cout << "fail to start listen!!\n" << "errcode: " << GetLastError() << endl;

		system("pause");
		return -1;
	}
	else { cout << "success start listen!\n"; }


	// 等待客户端连接
	while (true) {
		SOCKET newClientSocket = accept(listen_socket, NULL, NULL);  // 接收新的客户端连接

		// 限制连接数
		if (cur_connect < MaxClient) {  // 没满
			// 为这个客户端设置编号
			int num = 0;
			for (int i = 0; i < MaxClient; i++) {
				if (!clientflag[i]) {
					num = i;
					break;
				}
			}

			// 接受客户端连接
			/*SOCKET accept(
					SOCKET s,  // 监听socket
					struct sockaddr FAR * addr,  // 客户端IP地址与端口号
					int FAR * addrlen  // 结构的大小(与客户端共同选填)
			);*/
			clientSockets[num] = newClientSocket;
			clientflag[num] = 1;
			cur_connect++;
			// 告诉客户端连接被接受
			const char partMsg[] = "Connection accepted.Welcome!\nYou are Client ";
			char acceptMsg[buf_size];
			sprintf_s(acceptMsg, sizeof(acceptMsg), "%s: %d", partMsg, clientSockets[num]);
			send(clientSockets[num], acceptMsg, strlen(acceptMsg), 0);

			// 记录当前时间
			SYSTEMTIME st;
			GetLocalTime(&st);
			//  格式化时间字符串
			char timeStr[100];
			sprintf(timeStr, "[%04d-%02d-%02d %02d:%02d:%02d]",
				st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond);

			// 输出连接信息
			cout << "Client " << clientSockets[num] << " connected!! ---- " << timeStr << endl;
			cout << "Current connect num: " << cur_connect << endl;

			// 进入线程
			HANDLE Thread = CreateThread(NULL, 0, thread_func, (LPVOID)num, 0, NULL);
		    if(Thread == NULL){
				cout << "fail to create thread!!\n";

				system("pause");
				return -1;
			}
			else { CloseHandle(Thread); }
		}
		else { // 已达最大连接	
			cout << "Connect num Max!!!\n";
			const char errMsg[] = "Server is full. Connection rejected.\nMaybe you can come here later...";
			send(newClientSocket, errMsg, strlen(errMsg), 0);
			// 关闭新连接的客户端socket
			closesocket(newClientSocket);
		}  	
	}

	closesocket(listen_socket);
	WSACleanup();
    return 0;
}