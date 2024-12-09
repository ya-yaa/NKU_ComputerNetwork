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
const int MAXSIZE = 10240; // ���仺������󳤶�
const int RecvPort = 8888;
int WINDOWS = 10;  // ���ڴ�С

struct HEADER {
	uint16_t checksum;  // 16λУ���
	uint16_t length;  // ���ݳ���
	uint8_t seq;  // ���к�
	uint8_t ack;  // ȷ�����к�
	uint8_t flag;  // ��־λ
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

// ����У���
uint16_t checksum(HEADER header) {
	int size = sizeof(header);
	uint16_t* msg = (uint16_t*)&header;  // ���ṹ����Ϊ 16 λ��������

	int count = (size + 1) / 2;
	u_short* buf = (u_short*)malloc(size + 1);
	memset(buf, 0, size + 1); // ��ʼ��Ϊ0
	memcpy(buf, msg, size);

	uint32_t sum = 0;
	// ѭ���ۼ�ÿ�� 16 λ��
	for (int i = 0; i < count; i++) {
		sum += buf[i];
		if (sum & 0xffff0000) {
			sum &= 0xffff;
			sum++;
		}
	}
	return ~(sum & 0xffff);
}

// �������ֽ�������
bool Connect(SOCKET& recv_socket, SOCKADDR_IN& recv_addr, int& send_addrlen) {
	cout << "---------- ��ʼ�������� ----------" << endl;

	// ��socket��Ϊ������״̬��ʹ��������ִ�У����жϳ�ʱ�ش�
	u_long mode = 1;
	ioctlsocket(recv_socket, FIONBIO, &mode);

	// ���յ�һ��������Ϣ
	HEADER header1;
	char* buf1 = new char[sizeof(header1)];
	while (true) {
		int recvlength = recvfrom(recv_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, &send_addrlen);

		if (recvlength > 0) {
			memcpy(&header1, buf1, sizeof(header1));
			if (header1.flag == SYN && checksum(header1) == 0) {
				cout << "�յ���һ������" << endl;
				break;
			}
			else {
				cout << "��һ�����ִ���" << endl;
			}
		}
	}

	// �ڶ������֣�����SYN+ACK
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];
	header2.flag = ACK_SYN;  // ����flag
	header2.checksum = 0;
	header2.checksum = checksum(header2);  // ����У��� 
	memcpy(buf2, &header2, sizeof(header2));  // ���뻺����
	int tag = sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
	if (tag == -1) {  //��֤���ͽ��
		cout << "�ڶ������ַ���ʧ�ܣ�" << endl;
		return false;
	}
	else {
		cout << "�ڶ������ֳɹ�����" << endl;
	}
	clock_t start = clock();  // ��¼�ڶ�������ʱ��


	// ���������֣��ȴ�ACK
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	while (true) {
		int recvlength = recvfrom(recv_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, &send_addrlen);

		// ���յ������flag��У���
		if (recvlength > 0) {
			memcpy(&header3, buf3, sizeof(header3));
			if (header3.flag == ACK && checksum(header3) == 0) {
				cout << "�յ�����������" << endl;
				break;
			}
			else {
				cout << "�������������ݰ�����" << endl;
			}
		}

		// ÿ��ѭ���ж��Ƿ�ʱ
		if (clock() - start > MAX_TIME) {
			cout << "�ȴ����������ֳ�ʱ���ش��ڶ�������" << endl;
			tag = sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
			if (tag == -1) {  //��֤���ͽ��
				cout << "�ڶ��������ش�ʧ�ܣ�" << endl;
				return false;
			}
			else {
				cout << "�ڶ��������Ѿ��ش�" << endl;
			}
			start = clock();
		}
	}

	// �ָ�����ģʽ
	mode = 0;
	ioctlsocket(recv_socket, FIONBIO, &mode);
	cout << "---------- �������ֳɹ������ӳɹ���----------" << endl;
	return true;
}

// �Ĵλ��ֶϿ�����
bool DisConnect(SOCKET& recv_socket, SOCKADDR_IN& recv_addr, int& send_addrlen) {
	cout << "---------- ��ʼ���ֶϿ� ----------" << endl;

	// ��socket��Ϊ������״̬��ʹ��������ִ�У����жϳ�ʱ�ش�
	u_long mode = 1;
	ioctlsocket(recv_socket, FIONBIO, &mode);

	// ��һ�λ��֣��ȴ�FIN+ACK
	HEADER header1;
	char* buf1 = new char[sizeof(header1)];
	while (true) {
		int recvlength = recvfrom(recv_socket, buf1, sizeof(header1), 0, (sockaddr*)&recv_addr, &send_addrlen);

		if (recvlength > 0) {
			memcpy(&header1, buf1, sizeof(header1));
			if (header1.flag == FIN_ACK && checksum(header1) == 0) {
				cout << "�յ���һ�λ���" << endl;
				break;
			}
			else {
				cout << "��һ�λ��ִ���" << endl;
			}
		}
	}

	// �ڶ��λ��֣�����ACK
	HEADER header2;
	char* buf2 = new char[sizeof(header2)];
	header2.flag = ACK;  // ����flag
	header2.checksum = 0;
	header2.checksum = checksum(header2);  // ����У��� 
	memcpy(buf2, &header2, sizeof(header2));  // ���뻺����
	int tag = sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
	if (tag == -1) {  //��֤���ͽ��
		cout << "�ڶ��λ��ַ���ʧ�ܣ�" << endl;
		return false;
	}
	else {
		cout << "�ڶ��λ��ֳɹ�����" << endl;
	}

	// �����λ��֣�����FIN+ACK
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	header3.flag = FIN_ACK;  // ����flag
	header3.checksum = 0;
	header3.checksum = checksum(header3);  // ����У��� 
	memcpy(buf3, &header3, sizeof(header3));  // ���뻺����
	tag = sendto(recv_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, send_addrlen);
	if (tag == -1) {  //��֤���ͽ��
		cout << "�����λ��ַ���ʧ�ܣ�" << endl;
		return false;
	}
	else {
		cout << "�����λ��ֳɹ�����" << endl;
	}
	clock_t start = clock();  // ��¼�����λ���ʱ��

	// ���Ĵλ��֣��ȴ�ACK
	HEADER header4;
	char* buf4 = new char[sizeof(header4)];
	while (true) {
		int recvlength = recvfrom(recv_socket, buf4, sizeof(header4), 0, (sockaddr*)&recv_addr, &send_addrlen);

		// ���յ������flag��У���
		if (recvlength > 0) {
			memcpy(&header4, buf4, sizeof(header4));
			if (header4.flag == ACK && checksum(header4) == 0) {
				cout << "�յ����Ĵλ���" << endl;
				break;
			}
			else {
				cout << "���Ĵλ������ݰ�����" << endl;
			}
		}
		// ÿ��ѭ���ж��Ƿ�ʱ
		if (clock() - start > MAX_TIME) {
			cout << "---------- �ȴ����Ĵλ��ֳ�ʱ��ֱ�ӶϿ����� ----------" << endl;
			mode = 0;
			ioctlsocket(recv_socket, FIONBIO, &mode);
			return true;
		}
	}


	// �ָ�����ģʽ
	mode = 0;
	ioctlsocket(recv_socket, FIONBIO, &mode);
	cout << "---------- �Ĵλ��ֳɹ����Ͽ����ӳɹ��� ----------" << endl;
	return true;
}

// �����ļ�����
int Recv(SOCKET& recv_socket, SOCKADDR_IN& recv_addr, int& send_addrlen, char* message) {
	cout << "---- �����ļ����պ��� ----" << endl;

	// ��socket��Ϊ������״̬��ʹ��������ִ�У����жϳ�ʱ�ش�
	u_long mode = 1;
	ioctlsocket(recv_socket, FIONBIO, &mode);

	uint8_t ack_now = 0;
	uint8_t seq_now = 0;
	int length_now = 0;  // ��ǰ�յ������ݳ���

	// ѭ�������ļ���Ϣ
	HEADER header1;
	char* buf1 = new char[sizeof(header1) + MAXSIZE];
	while (1) {
		while (true) {
			int recvlength = recvfrom(recv_socket, buf1, sizeof(header1) + MAXSIZE, 0, (sockaddr*)&recv_addr, &send_addrlen);

			// �յ����У���
			if (recvlength > 0) {
				memcpy(&header1, buf1, sizeof(header1));
				if (checksum(header1) != 0) {
					cout << "У��ͳ���" << endl;
				}
				else {
					break;
				}
			}
		}

		// �ж��Ƿ��ǽ�����־
		if (header1.flag == OVER) {
			cout << "�յ��ļ����������־" << endl;
			break;
		}

		// ������ͨ���ݰ�
		HEADER header2;
		char* buf2 = new char[sizeof(header2)];
		if (header1.seq != ack_now) {  // �������������ݰ��������ظ�ACK
			header2.flag = 0;
			header2.ack = ack_now;
			header2.seq = seq_now;
			header2.checksum = checksum(header2);
			memcpy(buf2, &header2, sizeof(header2));
			sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
			cout << "�����յ� " << (int)ack_now << "  �����յ� " << static_cast<int>(header1.seq) << "  �����ظ�ack " << (int)header2.ack << endl;
		}
		else {  // �����������ݰ�
			// ȡ������
			int msg_len = header1.length;
			cout << "�յ���СΪ " << msg_len << " �ֽڵ�����" << endl;
			memcpy(message + length_now, buf1 + sizeof(header1), msg_len);
			length_now += msg_len;

			ack_now = (header1.seq + 1) % 256;
			seq_now = header1.seq;

			// �ظ���ӦACK
			header2.flag = 0;
			header2.ack = ack_now;
			header2.seq = seq_now;
			header2.checksum = 0;
			header2.checksum = checksum(header2);
			memcpy(buf2, &header2, sizeof(header2));
			sendto(recv_socket, buf2, sizeof(header2), 0, (sockaddr*)&recv_addr, send_addrlen);
			cout << "�ɹ��ظ�ACK " << static_cast<int>(header2.ack) << "  ��ǰ���к�SEQ " << static_cast<int>(header2.seq) << endl;
		}
	}
	cout << "ѭ�����ս���" << endl;

	// ѭ�����ս������������ݽ�����־
	HEADER header3;
	char* buf3 = new char[sizeof(header3)];
	header3.flag = OVER;
	header3.checksum = 0;
	header3.checksum = checksum(header3);
	memcpy(buf3, &header3, sizeof(header3));
	sendto(recv_socket, buf3, sizeof(header3), 0, (sockaddr*)&recv_addr, send_addrlen);
	cout << "�ѷ������ݴ��������־" << endl;

	// �ָ�����ģʽ
	mode = 0;
	ioctlsocket(recv_socket, FIONBIO, &mode);
	cout << "---- �ļ����պ������� ----" << endl;
	return length_now;
}

int main() {
	cout << "����ش�ʱ�䣺" << MAX_TIME << endl;

	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	struct sockaddr_in recv_addr;
	recv_addr.sin_family = AF_INET;  // IPV4
	recv_addr.sin_port = htons(RecvPort);
	inet_pton(AF_INET, "127.0.0.1", &recv_addr.sin_addr.s_addr);

	SOCKET recv = socket(AF_INET, SOCK_DGRAM, 0);

	if (bind(recv, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) == -1) {
		cout << "���׽���ʧ�ܣ�" << endl;
		return 1;
	}

	int len = sizeof(recv_addr);
	// ������������
	if (Connect(recv, recv_addr, len) == false) {
		cout << "����ʧ�ܣ���" << endl;
	}

	// ��������
	cout << "---------- ��ʼѭ�������ļ� ----------" << endl;
	char* filename = new char[20];
	char* filedata = new char[100000000];
	int namelen = Recv(recv, recv_addr, len, filename);
	int filelen = Recv(recv, recv_addr, len, filedata);

	string name(filename, namelen);
	cout << "���յ��ļ�����" << name << endl;
	cout << "���յ��ļ���С��" << filelen << "�ֽ�" << endl;
	ofstream file_stream(name, ios::binary); // �����ļ���
	file_stream.write(filedata, filelen);// д���ļ�����
	file_stream.close();
	cout << "---------- �ļ�������� -----------" << endl;


	// �Ĵλ��ֶϿ�����
	if (DisConnect(recv, recv_addr, len) == false) {
		cout << "�Ͽ�����ʧ�ܣ���" << endl;
	}

	system("pause");
	return 0;
}