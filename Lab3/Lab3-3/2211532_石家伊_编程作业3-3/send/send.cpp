#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <chrono>
#include<fstream>
#include <iomanip>
#include <time.h>
#include <thread>
#include <mutex>
#include <random>

#pragma comment(lib, "ws2_32.lib") 
using namespace std;

#define FIN 0x4  // 0100
#define ACK 0x2  // 0010
#define SYN 0x1  // 0001
#define ACK_SYN 0x3  // 0011
#define FIN_ACK 0x6  // 0110
#define OVER 0xF    // 1111

const double MAX_TIME = 0.5 * CLOCKS_PER_SEC;  // 等待最大时间
const int MAXSIZE = 10240; // 传输缓冲区最大长度
const int SendPort = 8888;  // 端口号
const int LossRate = 5;  //丢包率 LossRate/100

struct HEADER {
	uint16_t checksum;  // 16位校验和
	uint16_t length;  // 数据长度
	uint8_t seq;  // 序列号
	uint8_t ack;  // 确认序列号
	uint8_t flag;  // 标志位
	uint8_t temp;

	HEADER() {
		temp = 0;
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
bool Connect(SOCKET& send_socket, SOCKADDR_IN& recv_addr, int& recv_addrlen) {
	cout << "---------- 开始握手连接 ----------" << endl;

	// 将socket置为非阻塞状态，使函数继续执行，来判断超时重传
	u_long mode = 1;
	ioctlsocket(send_socket, FIONBIO, &mode);

	// 第一次握手，发送SYN
	HEADER header1;
	char* buf1 = new char[sizeof(header1)];
	header1.flag = SYN;  // 设置flag
	header1.checksum = 0;
	header1.checksum = checksum(header1);  // 计算校验和 
	memcpy(buf1, &header1, sizeof(header1));  // 放入缓冲区
	int tag = sendto(send_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, recv_addrlen);
	if (tag == -1) {  //验证发送结果
		cout << "第一次握手发送失败！" << endl;
		return false;
	}
	else {
		cout << "第一次握手成功发送 " << endl;
	}
	clock_t start = clock();  // 记录第一次握手时间

	// 第二次握手，等待SYN+ACK
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];
	while (true) {
		int recv_len = recvfrom(send_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		// 接收到，检查flag与校验和
		if (recv_len > 0) {
			memcpy(&header2, buf2, sizeof(header2));
			if (header2.flag == ACK_SYN && checksum(header2) == 0) {
				cout << "收到第二次握手" << endl;
				break;
			}
			else {
				cout << "第二次握手数据包出错！" << endl;
			}
		}

		// 每次循环判断是否超时
		if (clock() - start > MAX_TIME) {
			cout << "等待第二次握手超时！重传第一次握手" << endl;
			tag = sendto(send_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, recv_addrlen);
			if (tag == -1) {  //验证发送结果
				cout << "第一次握手重传失败！" << endl;
				return false;
			}
			else {
				cout << "第一次握手已经重传 " << endl;
			}
			start = clock();  // 更新握手时间
		}
	}

	// 第三次握手，发送ACK
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	header3.flag = ACK;  // 设置flag
	header3.checksum = 0;
	header3.checksum = checksum(header3);  // 计算校验和  
	memcpy(buf3, &header3, sizeof(header3));  // 放入缓冲区
	tag = sendto(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, recv_addrlen);
	if (tag == -1) {  //验证发送结果
		cout << "第三次握手发送失败！" << endl;
		return false;
	}
	else {
		cout << "第三次握手成功发送" << endl;
	}

	// 恢复阻塞模式
	mode = 0;
	ioctlsocket(send_socket, FIONBIO, &mode);
	// 三次握手结束，成功连接
	cout << "---------- 三次握手成功，连接成功！ ----------" << endl;
	return true;
}

// 四次挥手断开连接
bool DisConnect(SOCKET& send_socket, SOCKADDR_IN& recv_addr, int& recv_addrlen) {
	cout << "---------- 开始挥手断开 ----------" << endl;
	// 将socket置为非阻塞状态，使函数继续执行，来判断超时重传
	u_long mode = 1;
	ioctlsocket(send_socket, FIONBIO, &mode);

	// 第一次挥手，发送FIN+ACK
	HEADER header1;
	char* buf1 = new char[sizeof(header1)];
	header1.flag = FIN_ACK;  // 设置flag
	header1.checksum = 0;
	header1.checksum = checksum(header1);  // 计算校验和 
	memcpy(buf1, &header1, sizeof(header1));  // 放入缓冲区
	int tag = sendto(send_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, recv_addrlen);
	if (tag == -1) {  //验证发送结果
		cout << "第一次挥手发送失败！" << endl;
		return false;
	}
	else {
		cout << "第一次挥手成功发送 " << endl;
	}
	clock_t start = clock();  // 记录第一次挥手时间

	// 第二次挥手，等待ACK
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];
	while (true) {
		int recv_lenth = recvfrom(send_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		// 接收到，检查flag与校验和
		if (recv_lenth > 0) {
			memcpy(&header2, buf2, sizeof(header2));
			if (header2.flag == ACK && checksum(header2) == 0) {
				cout << "收到第二次挥手" << endl;
				break;
			}
			else {
				cout << "第二次挥手数据包出错！" << endl;
			}
		}

		// 每次循环判断是否超时
		if (clock() - start > MAX_TIME) {
			cout << "等待第二次挥手超时！重传第一次挥手" << endl;
			tag = sendto(send_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, recv_addrlen);
			if (tag == -1) {  //验证发送结果
				cout << "第一次挥手重传失败！" << endl;
				return false;
			}
			else {
				cout << "第一次挥手已经重传 " << endl;
			}
			start = clock();  // 更新挥手时间
		}
	}

	// 第三次挥手，等待FIN+ACK
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	start = clock();  // 更新挥手时间
	while (true) {
		int recvlength = recvfrom(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		// 接收到，检查flag与校验和
		if (recvlength > 0) {
			memcpy(&header3, buf3, sizeof(header3));
			if (header3.flag == FIN_ACK && checksum(header3) == 0) {
				cout << "收到第三次挥手" << endl;
				break;
			}
			else {
				cout << "第三次挥手数据包出错！" << endl;
			}
		}
	}

	// 第四次挥手，发送ACK
	HEADER header4;
	char* buf4 = new char[sizeof(header4)];
	header4.flag = ACK;  // 设置flag
	header4.checksum = 0;
	header4.checksum = checksum(header4);  // 计算校验和 
	memcpy(buf4, &header4, sizeof(header4));  // 放入缓冲区
	tag = sendto(send_socket, buf4, sizeof(header4), 0, (sockaddr*)&recv_addr, recv_addrlen);
	if (tag == -1) {  //验证发送结果
		cout << "第四次挥手发送失败！" << endl;
		return false;
	}
	else {
		cout << "第四次挥手成功发送 " << endl;
	}

	// 恢复阻塞模式
	mode = 0;
	ioctlsocket(send_socket, FIONBIO, &mode);
	cout << "---------- 四次挥手成功，断开连接成功！----------" << endl;
	return true;
}

mutex ack_mutex;  // 用于同步窗口更新
int head = 0;     // 窗口头部
int now = 0;      // 当前发送到的位置
int cwnd = 1;  // 窗口大小
int ssthresh = 16;
int last_ack = 0;  // 当前最后一次收到的ack
int lastack_flag = 0;  // 是否为重复ack 0表示不是
bool need_resend = 0;  // 用于ack冗余重传

// 接收ACK线程函数
void ReceiveACK(SOCKET& send_socket, SOCKADDR_IN& recv_addr, int& recv_addrlen, int& head, int times) {
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];

	int temp = 1;

	while (head < times) {
		int recvlength = recvfrom(send_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		if (recvlength > 0) {  // 收到
			memcpy(&header2, buf2, sizeof(header2));
			// 检查校验和
			if (checksum(header2) != 0) {
				cout << "校验和错误" << endl;
				continue;
			}
			else {
				if ((int)header2.ack != last_ack) {  // 收到正确ack 
					int step = ((int)header2.ack - (head % 256) + 256) % 256;

					if (lastack_flag != 0) {  // 上一个收到的ack是重复ack，快速回复刚结束
						lock_guard<mutex> lock(ack_mutex);  // 锁，要更改窗口
						head = head + step;
						cwnd = ssthresh;
						cout << "[接收] (快速恢复结束)  收到正确ack：" << (int)header2.ack << endl 
							<< "        窗口更新："  << "  当前窗口头：" << head << "  cwnd：" << cwnd << "  ssthresh：" << ssthresh << endl;

					}
					else if (cwnd < ssthresh) {  // 慢启动
						lock_guard<mutex> lock(ack_mutex);  // 锁，要更改窗口
						head = head + step;
						cwnd++;
						cout << "[接收] (慢启动中)  收到正确ack：" << (int)header2.ack << endl 
							<< "        窗口更新" << "  当前窗口头：" << head << "  大小：" << cwnd << "  ssthresh：" << ssthresh << endl;
					}
					else {  // 拥塞避免
						if (temp >= cwnd) {
							lock_guard<mutex> lock(ack_mutex);  // 锁，要更改窗口
							head = head + step;
							cwnd++;
							cout << "[接收] (拥塞避免中，窗口增大)  收到正确ack：" << (int)header2.ack << endl 
								<< "        窗口更新" << "  当前窗口头：" << head << "  大小：" << cwnd << "  temp：" << temp << "  ssthresh：" << ssthresh << endl;
							temp = 1;
						}
						else {
							lock_guard<mutex> lock(ack_mutex);  // 锁，要更改窗口
							head = head + step;
							temp++;
							cout << "[接收] (拥塞避免中，窗口大小不变)  收到正确ack：" << (int)header2.ack << endl 
								<< "        窗口更新" << "  当前窗口头：" << head << "  大小：" << cwnd << "  temp：" << temp << "  ssthresh：" << ssthresh << endl;
						}
					}

					// 更新最后一次ack状态
					last_ack = (int)header2.ack;
					lastack_flag = 0;
				}
				else {  // 重复ack
					lastack_flag++;
					if (lastack_flag < 3) {
						lock_guard<mutex> lock(ack_mutex);
						cout << "[接收重复ack]  " << last_ack << "  lastack_flag:" << lastack_flag << "  当前窗口头：" << head << "  cwnd：" << cwnd << "  ssthresh：" << ssthresh << endl;
					}
					else if (lastack_flag == 3) {  // 收到三次重复ack
						lock_guard<mutex> lock(ack_mutex);
						ssthresh = cwnd / 2;
						cwnd = ssthresh + 3;
						need_resend = 1;
						cout << "[接收重复ack]  " << last_ack << "  lastack_flag:" << lastack_flag << "  当前窗口头：" << head << "  cwnd：" << cwnd << "  ssthresh：" << ssthresh << endl;
					}
					else if (lastack_flag > 3) {
						lock_guard<mutex> lock(ack_mutex);
						// 设置一个快速增长的上限，防止重传的第一个包就丢失
						if (lastack_flag > ssthresh + 3) {
							cout << "[接收重复ack到达上限！！窗口大小不再增加]" << "  当前窗口头：" << head << "  cwnd：" << cwnd << "  ssthresh：" << ssthresh << endl;
							continue;
						}
						
						cwnd++;
						cout << "[接收重复ack]  " << last_ack << "  lastack_flag:" << lastack_flag << "  当前窗口头：" << head << "  cwnd：" << cwnd << "  ssthresh：" << ssthresh << endl;
					}
				}
			}
		}
	}
}

// 数据传输
bool Send(SOCKET& send_socket, SOCKADDR_IN& recv_addr, int& recv_addrlen, char* message, int msg_len) {
	cout << "---- 进入文件传输函数 ----" << endl;

	// 初始化状态
	head = 0;     // 已发送未确认的头部
	now = 0;      // 当前发送的序列号
	cwnd = 1;  // 窗口大小
	ssthresh = 16;
	last_ack = 0;  // 当前最后一次收到的ack
	lastack_flag = 0;  // 是否为重复ack 0表示不是

	int times = msg_len / MAXSIZE + (msg_len % MAXSIZE != 0);  // 需要发送的次数，向上取整

	cout << "文件大小 " << msg_len << " 字节" << "需要传输 " << times << " 次" << endl;

	clock_t start = 0;  // 记录窗口发送时间

	// 将socket置为非阻塞状态，使函数继续执行，来判断超时重传
	u_long mode = 1;
	ioctlsocket(send_socket, FIONBIO, &mode);

	// 启动接收ACK的线程
	thread ack_thread(ReceiveACK, ref(send_socket), ref(recv_addr), ref(recv_addrlen), ref(head), times);

	// 创建一个默认的随机数引擎用于丢包
	std::default_random_engine engine;
	// 初始化随机数引擎，使用当前时间作为种子
	std::seed_seq seed{ static_cast<long unsigned int>(std::time(0)) };
	engine.seed(seed);
	// 0-99之间取随机数
	std::uniform_int_distribution<int> distribution(0, 99);

	// 开始窗口移动，右移直到结束
	while (head < times) {
		// 操作当前窗口，循环发送head开始cwnd大小的内容
		for (now; now < head + cwnd && now < times; now++) {
			if (need_resend == 1) {
				lock_guard<mutex> lock(ack_mutex);
				cout << "[冗余ack重传] lastack_flag:" << lastack_flag << "  当前窗口头：" << head << "  大小：" << cwnd << endl;
				now = head;
				need_resend = 0;
			}

			// 生成随机数，以LossRate的概率丢包
			int randomNumber = distribution(engine);
			if (randomNumber < LossRate) {
				lock_guard<mutex> lock(ack_mutex);
				cout << "[丢包!!!] 窗口头:" << head << " 大小：" << cwnd << "  now : " << now << "  seq : " << now % 256 << endl;
				continue;
			}

			// 计算本次传输数据大小
			int len = (now == times - 1) ? (msg_len - MAXSIZE * (times - 1)) : MAXSIZE;

			// 设置header
			HEADER header1;
			char* buf1 = new char[sizeof(header1) + len];
			header1.flag = 0;
			header1.length = len;
			header1.seq = now % 256;
			header1.checksum = 0;
			header1.checksum = checksum(header1);
			memcpy(buf1, &header1, sizeof(header1));

			// 设置信息
			char* msg = &message[now * MAXSIZE];
			memcpy(buf1 + sizeof(header1), msg, len);

			// 发送
			lock_guard<mutex> lock(ack_mutex);
			cout << "[发送] 发送窗口头:" << head << " 大小：" << cwnd << "  now: " << now << "  seq: " << now % 256 << endl;
			sendto(send_socket, buf1, sizeof(header1) + len, 0, (sockaddr*)&recv_addr, recv_addrlen);
			start = clock();  // 记录数据发送时间
		}
		if (clock() - start > MAX_TIME) {  // 判断超时
			lock_guard<mutex> lock(ack_mutex);
			cout << "等待窗口ACK超时！重传" << endl;
			now = head;
			ssthresh = cwnd / 2;
			cwnd = 1;
			lastack_flag = 0;  // 确保进入慢启动，而不是快速恢复结束
			start = clock();
		}
	}

	ack_thread.join();  // 等待线程处理完全部ACK

	cout << "文件内容已全部发送" << endl;

	// 发送结束标志
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	header3.flag = OVER;  // 设置flag
	header3.checksum = 0;
	header3.checksum = checksum(header3);  // 计算校验和 
	memcpy(buf3, &header3, sizeof(header3));  // 放入缓冲区

	sendto(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, recv_addrlen);

	start = clock();  // 记录时间

	// 等待结束标志确认
	while (true) {
		int recvlength = recvfrom(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, &recv_addrlen);

		// 接收到，检查标志位与校验和
		if (recvlength > 0) {
			memcpy(&header3, buf3, sizeof(header3));
			if (header3.flag == OVER && checksum(header3) == 0) {
				cout << "收到结束标志ACK" << endl;
				break;
			}
			else {
				cout << "结束标志ACK出错！" << endl;
			}
		}

		if (clock() - start > MAX_TIME) {
			cout << "等待结束标志确认超时！重传" << endl;
			sendto(send_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, recv_addrlen);
			start = clock();  // 记录时间
		}
	}

	// 恢复阻塞模式
	mode = 0;
	ioctlsocket(send_socket, FIONBIO, &mode);
	// 文件传输结束
	cout << "---- 文件传输函数结束 ----" << endl;
	return true;
}

int main() {
	cout << "最大重传时间：" << MAX_TIME << endl;

	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	struct sockaddr_in recv_addr;
	recv_addr.sin_family = AF_INET;  // IPV4
	recv_addr.sin_port = htons(SendPort);
	inet_pton(AF_INET, "127.0.0.1", &recv_addr.sin_addr.s_addr);

	SOCKET send = socket(AF_INET, SOCK_DGRAM, 0);

	int len = sizeof(recv_addr);

	// 三次握手连接
	if (Connect(send, recv_addr, len) == false) {
		cout << "连接失败！!" << endl;
		return 0;
	}

	bool flag;
	cout << "请输入操作码：\n" << "0：退出  " << "1：传输文件" << endl;
	cin >> flag;
	if (flag == true) {  // 传输文件
		cout << "请输入你要传入的文件名：";
		string filename;
		cin >> filename;

		ifstream fileIN(filename.c_str(), ifstream::binary); // 以二进制方式打开文件
		// 读入文件内容
		char* buf = new char[100000000];
		int i = 0;
		unsigned char temp = fileIN.get();
		while (fileIN)
		{
			buf[i++] = temp;
			temp = fileIN.get();
		}
		fileIN.close();

		// 开始发送
		cout << "---------- 开始传输文件 ----------" << endl;
		clock_t begin = clock(); // 起始时间

		// 发送文件名
		cout << "文件名传输：" << endl;
		Send(send, recv_addr, len, (char*)(filename.c_str()), filename.length());
		// 发送文件内容
		cout << "文件内容传输：" << endl;
		Send(send, recv_addr, len, buf, i);

		clock_t end = clock(); // 结束时间

		// 计算时间差并输出，单位为秒
		double elapsed_time = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
		cout << "文件传输消耗的时间: " << elapsed_time << " 秒" << endl;
		// 计算吞吐率
		double throughput = (i + filename.length()) / elapsed_time;
		cout << "文件传输吞吐率: " << throughput << " byte/s" << endl;
		cout << "---------- 文件传输完成 ----------" << endl;

	}
	else {  // 发送两次文件结束标志
		u_long mode = 1;
		ioctlsocket(send, FIONBIO, &mode);

		HEADER header1;
		char* buf1 = new char[sizeof(header1)];
		header1.flag = OVER;  // 设置flag
		header1.checksum = 0;
		header1.checksum = checksum(header1);  // 计算校验和 
		memcpy(buf1, &header1, sizeof(header1));  // 放入缓冲区

		//// 结束标志1
		sendto(send, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, len);
		cout << "结束标志1发送成功" << endl;
		clock_t start = clock();  // 记录时间
		// 等待结束标志确认
		HEADER header2;
		char* buf2 = new char[sizeof(header2)];
		while (true) {
			int recvlength = recvfrom(send, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &len);

			// 接收到，检查标志位与校验和
			if (recvlength > 0) {
				memcpy(&header2, buf2, sizeof(header2));
				if (header2.flag == OVER && checksum(header2) == 0) {
					cout << "收到结束标志1ACK" << endl;
					break;
				}
				else {
					cout << "结束标志1ACK出错！" << endl;
				}
			}

			if (clock() - start > MAX_TIME) {
				cout << "等待结束标志1确认超时！重传一次" << endl;
				sendto(send, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, len);
				cout << "结束标志1重传成功" << endl;
				start = clock();  // 记录时间
			}
		}

		//// 结束标志2
		sendto(send, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, len);
		cout << "结束标志2发送成功" << endl;
		start = clock();  // 记录时间
		// 等待结束标志确认
		while (true) {
			int recvlength = recvfrom(send, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, &len);

			// 接收到，检查标志位与校验和
			if (recvlength > 0) {
				memcpy(&header2, buf2, sizeof(header2));
				if (header2.flag == OVER && checksum(header2) == 0) {
					cout << "收到结束标志2ACK" << endl;
					break;
				}
				else {
					cout << "结束标志2ACK出错！" << endl;
				}

			}
			if (clock() - start > MAX_TIME) {
				cout << "等待结束标志2确认超时！重传一次" << endl;
				sendto(send, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, len);
				cout << "结束标志2重传成功" << endl;
				start = clock();  // 记录时间
			}
		}

		// 恢复阻塞模式
		mode = 0;
		ioctlsocket(send, FIONBIO, &mode);
	}

	// 四次挥手断开连接
	if (DisConnect(send, recv_addr, len) == false) {
		cout << "断开连接失败！!" << endl;
		return 0;
	}

	system("pause");
	return 0;
}