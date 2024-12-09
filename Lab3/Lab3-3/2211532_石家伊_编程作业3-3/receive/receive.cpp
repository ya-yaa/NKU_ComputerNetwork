#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <chrono>
#include<fstream>
#include <iomanip>
#include <time.h>

#pragma comment(lib, "ws2_32.lib") 
using namespace std;

#define FIN 0x4  // 0100
#define ACK 0x2  // 0010
#define SYN 0x1  // 0001
#define ACK_SYN 0x3  // 0011
#define FIN_ACK 0x6  // 0110
#define ALL_OVER 0x0  // 0000
#define OVER 0xF    // 1111

const double MAX_TIME = 0.5 * CLOCKS_PER_SEC;
const int MAXSIZE = 10240; // 传输缓冲区最大长度
const int RecvPort = 8888;
int WINDOWS = 10;  // 窗口大小

struct HEADER {
	uint16_t checksum;  // 16位校验和
	uint16_t length;  // 数据长度
	uint8_t seq;  // 序列号
	uint8_t ack;  // 确认序列号
	uint8_t flag;  // 标志位
	uint8_t tmp;

	HEADER() {
		tmp = 0;
		checksum = 0;
		seq = 0;
		ack = 0;
		flag = 0;
		length = 0;
	}
};
#pragma pack(1)

// 计算校验和
uint16_t checksum(HEADER header) {
	int size = sizeof(header);
	uint16_t* msg = (uint16_t*)&header;  // 将结构体视为 16 位整数数组

	int count = (size + 1) / 2;
	u_short* buf = (u_short*)malloc(size + 1);
	memset(buf, 0, size + 1); // 初始化为0
	memcpy(buf, msg, size);

	uint32_t sum = 0;
	// 循环累加每个 16 位段
	for (int i = 0; i < count; i++) {
		sum += buf[i];
		if (sum & 0xffff0000) {
			sum &= 0xffff;
			sum++;
		}
	}
	return ~(sum & 0xffff);
}

// 三次握手建立连接
bool Connect(SOCKET& recv_socket, SOCKADDR_IN& recv_addr, int& send_addrlen) {
	cout << "---------- 开始握手连接 ----------" << endl;

	// 将socket置为非阻塞状态，使函数继续执行，来判断超时重传
	u_long mode = 1;
	ioctlsocket(recv_socket, FIONBIO, &mode);

	// 接收第一次握手信息
	HEADER header1;
	char* buf1 = new char[sizeof(header1)];
	while (true) {
		int recvlength = recvfrom(recv_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, &send_addrlen);

		if (recvlength > 0) {
			memcpy(&header1, buf1, sizeof(header1));
			if (header1.flag == SYN && checksum(header1) == 0) {
				cout << "收到第一次握手" << endl;
				break;
			}
			else {
				cout << "第一次握手错误！" << endl;
			}
		}
	}

	// 第二次握手，发送SYN+ACK
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];
	header2.flag = ACK_SYN;  // 设置flag
	header2.checksum = 0;
	header2.checksum = checksum(header2);  // 计算校验和 
	memcpy(buf2, &header2, sizeof(header2));  // 放入缓冲区
	int tag = sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
	if (tag == -1) {  //验证发送结果
		cout << "第二次握手发送失败！" << endl;
		return false;
	}
	else {
		cout << "第二次握手成功发送" << endl;
	}
	clock_t start = clock();  // 记录第二次握手时间


	// 第三次握手，等待ACK
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	while (true) {
		int recvlength = recvfrom(recv_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, &send_addrlen);

		// 接收到，检查flag与校验和
		if (recvlength > 0) {
			memcpy(&header3, buf3, sizeof(header3));
			if (header3.flag == ACK && checksum(header3) == 0) {
				cout << "收到第三次握手" << endl;
				break;
			}
			else {
				cout << "第三次握手数据包出错！" << endl;
			}
		}

		// 每次循环判断是否超时
		if (clock() - start > MAX_TIME) {
			cout << "等待第三次握手超时！重传第二次握手" << endl;
			tag = sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
			if (tag == -1) {  //验证发送结果
				cout << "第二次握手重传失败！" << endl;
				return false;
			}
			else {
				cout << "第二次握手已经重传" << endl;
			}
			start = clock();
		}
	}

	// 恢复阻塞模式
	mode = 0;
	ioctlsocket(recv_socket, FIONBIO, &mode);
	cout << "---------- 三次握手成功，连接成功！----------" << endl;
	return true;
}

// 四次挥手断开连接
bool DisConnect(SOCKET& recv_socket, SOCKADDR_IN& recv_addr, int& send_addrlen) {
	cout << "---------- 开始挥手断开 ----------" << endl;

	// 将socket置为非阻塞状态，使函数继续执行，来判断超时重传
	u_long mode = 1;
	ioctlsocket(recv_socket, FIONBIO, &mode);

	// 第一次挥手，等待FIN+ACK
	HEADER header1;
	char* buf1 = new char[sizeof(header1)];
	while (true) {
		int recvlength = recvfrom(recv_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, &send_addrlen);

		if (recvlength > 0) {
			memcpy(&header1, buf1, sizeof(header1));
			if (header1.flag == FIN_ACK && checksum(header1) == 0) {
				cout << "收到第一次挥手" << endl;
				break;
			}
			else {
				cout << "第一次挥手错误！" << endl;
			}
		}
	}

	// 第二次挥手，发送ACK
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];
	header2.flag = ACK;  // 设置flag
	header2.checksum = 0;
	header2.checksum = checksum(header2);  // 计算校验和 
	memcpy(buf2, &header2, sizeof(header2));  // 放入缓冲区
	int tag = sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
	if (tag == -1) {  //验证发送结果
		cout << "第二次挥手发送失败！" << endl;
		return false;
	}
	else {
		cout << "第二次挥手成功发送" << endl;
	}

	// 第三次挥手，发送FIN+ACK
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	header3.flag = FIN_ACK;  // 设置flag
	header3.checksum = 0;
	header3.checksum = checksum(header3);  // 计算校验和 
	memcpy(buf3, &header3, sizeof(header3));  // 放入缓冲区
	tag = sendto(recv_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, send_addrlen);
	if (tag == -1) {  //验证发送结果
		cout << "第三次挥手发送失败！" << endl;
		return false;
	}
	else {
		cout << "第三次挥手成功发送" << endl;
	}
	clock_t start = clock();  // 记录第三次挥手时间

	// 第四次挥手，等待ACK
	HEADER header4;
	char* buf4 = new char[sizeof(header4)];
	while (true) {
		int recvlength = recvfrom(recv_socket, buf4, sizeof(header4), 0, (sockaddr*)&recv_addr, &send_addrlen);

		// 接收到，检查flag与校验和
		if (recvlength > 0) {
			memcpy(&header4, buf4, sizeof(header4));
			if (header4.flag == ACK && checksum(header4) == 0) {
				cout << "收到第四次挥手" << endl;
				break;
			}
			else {
				cout << "第四次挥手数据包出错！" << endl;
			}
		}
		// 每次循环判断是否超时
		if (clock() - start > MAX_TIME) {
			cout << "---------- 等待第四次挥手超时！直接断开！！ ----------" << endl;
			mode = 0;
			ioctlsocket(recv_socket, FIONBIO, &mode);
			return true;
		}
	}


	// 恢复阻塞模式
	mode = 0;
	ioctlsocket(recv_socket, FIONBIO, &mode);
	cout << "---------- 四次挥手成功，断开连接成功！ ----------" << endl;
	return true;
}

// 接收文件函数
int Recv(SOCKET& recv_socket, SOCKADDR_IN& recv_addr, int& send_addrlen, char* message) {
	cout << "---- 进入文件接收函数 ----" << endl;

	// 将socket置为非阻塞状态，使函数继续执行，来判断超时重传
	u_long mode = 1;
	ioctlsocket(recv_socket, FIONBIO, &mode);

	uint8_t ack_now = 0;
	uint8_t seq_now = 0;
	int length_now = 0;  // 当前收到的数据长度

	// 循环接受文件信息
	HEADER header1;
	char* buf1 = new char[sizeof(header1) + MAXSIZE];
	while (1) {
		while (true) {
			int recvlength = recvfrom(recv_socket, buf1, sizeof(header1) + MAXSIZE, 0, (sockaddr*)&recv_addr, &send_addrlen);

			// 收到检查校验和
			if (recvlength > 0) {
				memcpy(&header1, buf1, sizeof(header1));
				if (checksum(header1) != 0) {
					cout << "校验和出错！" << endl;
				}
				else {
					break;
				}
			}
		}

		// 判断是否是结束标志
		if (header1.flag == OVER) {
			cout << "收到文件传输结束标志" << endl;
			break;
		}

		// 处理普通数据包
		HEADER header2;
		char* buf2 = new char[sizeof(header2)];
		if (header1.seq != ack_now) {  // 不是期望的数据包，发送重复ACK
			header2.flag = 0;
			header2.ack = ack_now;
			header2.seq = seq_now;
			header2.checksum = checksum(header2);
			memcpy(buf2, &header2, sizeof(header2));
			sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
			cout << "期望收到 " << (int)ack_now << "  本次收到 " << static_cast<int>(header1.seq) << "  发送重复ack " << (int)header2.ack << endl;
		}
		else {  // 是期望的数据包
			// 取出数据
			int msg_len = header1.length;
			cout << "收到大小为 " << msg_len << " 字节的数据" << endl;
			memcpy(message + length_now, buf1 + sizeof(header1), msg_len);
			length_now += msg_len;

			ack_now = (header1.seq + 1) % 256;
			seq_now = header1.seq;

			// 回复对应ACK
			header2.flag = 0;
			header2.ack = ack_now;
			header2.seq = seq_now;
			header2.checksum = 0;
			header2.checksum = checksum(header2);
			memcpy(buf2, &header2, sizeof(header2));
			sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
			cout << "成功回复ACK " << static_cast<int>(header2.ack) << "  当前序列号SEQ " << static_cast<int>(header2.seq) << endl;
		}
	}
	cout << "循环接收结束" << endl;

	// 循环接收结束，发送数据结束标志
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	header3.flag = OVER;
	header3.checksum = 0;
	header3.checksum = checksum(header3);
	memcpy(buf3, &header3, sizeof(header3));
	sendto(recv_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, send_addrlen);
	cout << "已发送数据传输结束标志" << endl;

	// 恢复阻塞模式
	mode = 0;
	ioctlsocket(recv_socket, FIONBIO, &mode);
	cout << "---- 文件接收函数结束 ----" << endl;
	return length_now;
}

int main() {
	cout << "最大重传时间：" << MAX_TIME << endl;

	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	struct sockaddr_in recv_addr;
	recv_addr.sin_family = AF_INET;  // IPV4
	recv_addr.sin_port = htons(RecvPort);
	inet_pton(AF_INET, "127.0.0.1", &recv_addr.sin_addr.s_addr);

	SOCKET recv = socket(AF_INET, SOCK_DGRAM, 0);

	if (bind(recv, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) == -1) {
		cout << "绑定套接字失败！" << endl;
		return 1;
	}

	int len = sizeof(recv_addr);
	// 三次握手连接
	if (Connect(recv, recv_addr, len) == false) {
		cout << "连接失败！！" << endl;
	}

	// 接收数据
	cout << "---------- 开始循环接收文件 ----------" << endl;
	char* filename = new char[20];
	char* filedata = new char[100000000];
	int namelen = Recv(recv, recv_addr, len, filename);
	int filelen = Recv(recv, recv_addr, len, filedata);

	string name(filename, namelen);
	cout << "接收到文件名：" << name << endl;
	cout << "接收到文件大小：" << filelen << "字节" << endl;
	ofstream file_stream(name, ios::binary); // 创建文件流
	file_stream.write(filedata, filelen);// 写入文件内容
	file_stream.close();
	cout << "---------- 文件接收完毕 -----------" << endl;


	// 四次挥手断开连接
	if (DisConnect(recv, recv_addr, len) == false) {
		cout << "断开连接失败！！" << endl;
	}

	system("pause");
	return 0;
}