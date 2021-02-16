# CS144

## lab0

### 环境配置

参考

### 写webget

1. 先熟悉给的api，了解FileDescriptor，Socket，TCPSocket，Address的使用方法，下面的实例很有用
2. 仿照handout前面用telnet手动连接的过程把代码写出来
3. handout步骤里的Hint很重要
   1. 请求完毕后别忘了shutdown来告知server我要发的都发了，你可以关了
   2. 传过来的字节流是以socket达到EOF为结束标志的，也就是说到EOF之前，我们需要多次read，一个read是不够的

```cpp
    const Address server(host, "http");  // can't be https
    // create socket
    TCPSocket sock;
    // connect to server
    sock.connect(server);
    // send http request
    sock.write("GET "+ path + " HTTP/1.1\r\n");
    sock.write("Host: " + host + "\r\n");
    sock.write("\r\n");
    // close socket
    sock.shutdown(SHUT_WR);
    // reveive data
    while (!sock.eof()) {  // if only one sock.read() , can't read all data 
        cout << sock.read();
    }
    

```



### 写in-memory reliable byte stream

1. 正确理解之前的东西，并对c++ 基本语法，string lib有一定了解之后可以开始写
2. 先思考用什么来装这个byte stream，头文件的注释里说了不用太精细的结构，所以我直接用的string来模拟一个队列
3. 接口主要分为两部分，writer和reader
4. 初始化就是确定这个bytestream的容量
5. 主要的逻辑是往string写（push_back)和从string读（copy and erase）
6. 其次就是eof，eof注释中说的是 `true if the output has reached the ending`，也就是说读完了，再翻译一下就是writer说我不写了 && bytesteam is empty
7. 细枝末节就是维护capacity，buffer，writenCount，readCount，endwrite几个小变量

```cpp
// 头文件中
  private:
    // Your code here -- add private members as necessary.

    // Hint: This doesn't need to be a sophisticated data structure at
    // all, but if any of your tests are taking longer than a second,
    // that's a sign that you probably want to keep exploring
    // different approaches.

    
    bool _eof = false;          
    bool _closed = false;       
    unsigned _read_count = 0;   
    unsigned _write_count = 0;  
    bool _end_write = false;
    unsigned _capacity = 0;
    unsigned _buffer = 0;
    std::string stream = "";

    bool _error{};  //!< Flag indicating that the stream suffered an error.
// cc中
ByteStream::ByteStream(const size_t capacity) { 
	this->_capacity = capacity; 
}

size_t ByteStream::write(const string &data) {
    unsigned int i;
    for (i = 0; i < data.length() && this->_buffer < this->_capacity; i++, this->_buffer++) {
    	this->stream.push_back(data[i]);
    }
    this->_write_count += i; // write i so add i
    return {i};
}

string ByteStream::peek_output(const size_t len) const {
    return {this->stream.substr(0, len)};
}

void ByteStream::pop_output(const size_t len) { 
	this->stream.erase(0, len);
	this->_buffer -= len;
	this->_read_count += len;   // from the testcase can we know that 
								// _read_count have to be updated here
}

std::string ByteStream::read(const size_t len) {
    string result = peek_output(len);
    pop_output(len);
    return {result};
}

void ByteStream::end_input() {this->_end_write = true;}

bool ByteStream::input_ended() const { return {this->_end_write}; }

size_t ByteStream::buffer_size() const { return {this->_buffer}; }

bool ByteStream::buffer_empty() const { return {this->_buffer == 0}; }

bool ByteStream::eof() const { return {input_ended() && buffer_empty()}; }

size_t ByteStream::bytes_written() const { return {this->_write_count}; }

size_t ByteStream::bytes_read() const { return {this->_read_count}; }

size_t ByteStream::remaining_capacity() const { return {_capacity - _buffer}; }

```

## lab1

### 写StreamReassembler

先看api

```cpp
// Construct a `StreamReassembler` that will store up to `capacity` bytes. 
StreamReassembler(const size_t capacity);
// Receive a substring and write any newly contiguous bytes into the stream. 
// 
// `data`: the segment 
// `index` indicates the index (place in sequence) of the first byte in `data` 
// `eof`: the last byte of this segment is the last byte in the entire stream
void push_substring(const string &data, const uint64_t index, const bool eof);
// Access the reassembled byte stream ByteStream &stream_out();
// The number of bytes in the substrings stored but not yet reassembled size_t 
unassembled_bytes() const;
// Is the internal state empty (other than the output stream)? 
bool empty() const;
```

简而言之，就是我们要写一个用来组装segment的Reassembler，他要干什么呢？

1. 我们知道传来的segment有3个属性
   1. data：装的数据
   2. index：数据的起始index
   3. eof：该片段是否为最后一部分
2. 组装器要做的就是两件事
   1. 接受segment（意味着要存储
   2. `supply a ByteStream with all of the data correctly ordered`，对于已经到达的**有序数据**全部写入`ByteSteam`，从`Hint`可以直到什么时候**不该**把`bytes`写到`stream`中：当且仅当这个byte前面的byte都还没写到steam中
3. 有一个需要处理的地方是segment有可能重叠

#### 疑惑

1. 一个reassembler 可以不停的组装segment？
2. reassembler的capacity和output的capacity什么关系
3. 如何处理空segment，空segment可以为eof
4. 如果capacity只剩3，然后到来的数据大小为5，但他包括了我想要的大小为3的数据，咋办，扔掉还是去头去尾
5. reassembler的capacity可以满吗？满了不就代表无法接受新的segment，如果此时reassembler里没有lookfor，会不会导致死锁
6. 

#### 待处理问题

1. 我现在的data是用vector写的，只有存，没有在把他写入Stream之后删掉，导致了空间不够，要换成list，修复这一bug，这使我的single test最后eof过不了
2. test 22 （8/16）也是因为后来的数据由于capacity已满进不去导致的
3. 简而言之就是数据包的整体大小大于我的capacity，所以_valid也要想办法替换
4. insert 不能插到末尾
5. test19 和 21 超时，可能是因为我的写是一个一个字符写的，原本api的参数是string，所以我想改成用list或别的容器直接装string，但是不知道怎么把原data的已有char去掉然后coalesce到list中

#### 收获

1. 迭代器会失效
2. substr第二个参数是count
3. 重写struct的比较运算符， const参数 对应const函数
4. set的迭代器访问前一个用 `--it` 而不是 `it--`, `--begin()` 依旧是`begin()`
5. hh文件里的private结构体在cc文件里用要加上类名比如 StreamReassembler::Segment
6. 原来迭代器还有倒着来的。。。
7. 



单个char存储在deque的版本

头文件

```cpp
#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <set>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
using namespace std;

class StreamReassembler {
    private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    std::deque<pair<char, size_t>> _data{};
    std::set<size_t> _valid{};
    // unsigned _unassembled_bytes = 0;
    size_t lookfor = 0;
    
    size_t endindex = 0;

    std::pair<string, size_t> get_useful_data(const std::string &data, const size_t index) const {
        // trim data to [start, end), which is what we need 
        size_t start;
        for (start = 0; start != data.length(); start++) {
            if (_valid.count(index + start) == 1) { // existed 
                continue;
            } else {
                break;
            }
        }

        size_t end = data.length();
        while (end != start) {
            if (_valid.count(index + end-1) == 0) break;  // not existed
            else end--;
        }

        if (end == start) return make_pair("", _capacity + 100);
        else return make_pair(data.substr(start, end-start), start + index);
    }
    // std::pair<string, size_t> get_useful_data(const std::string &data, const size_t index) const {
    //     string str = "";
    //     for (start = 0; start != data.length(); start++) {
    //         if (_valid.count(index + start) == 1) { // existed 
    //             continue;
    //         } else {
    //             str += data[start];
    //         }
    //     }
    //     if (str.length() == 0) return make_pair("", _capacity + 100);
    //     return make_pair(str, start + index);
    // }
    size_t get_insert_index(const size_t idx) const {
        if (_data.size() == 0) return 0;
        for (size_t i = 0; i < _data.size(); i++) {
            if (_data[i].second > idx ) return i ;
        }  
        return _data.size();
    }


  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

```

cc

```cpp
#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
	this->endindex = _capacity + 100; // im not sure if its correct style
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
	// if reassembler is full, return
	if (_data.size() == _capacity) return;
	// if stream is full, return
	if (_output.remaining_capacity() == 0) return;

	// handle empty but eof
	// update eof and endindex
    if (eof) {
    	endindex = index + data.length();
    } 

    // check if arrive endindex
    if (lookfor == endindex) { // handle segment like ("", 0, true)
    	_output.end_input();
    	return;
    }

	// trim data to [start, end), which is what we need, return pair<newdata, newindex>
	auto useful_data = get_useful_data(data, index);

	// if no useful_data, then drop this segment
	if (useful_data.first.length() == 0) return;

	// find insert index
	size_t idx = get_insert_index(useful_data.second);
	if (idx == _capacity + 100) return; // invalid idx

    // insert useful data into _data
    // considering _data's capacity

    for (size_t i = 0; i != useful_data.first.length() && _data.size() != _capacity; i++) {
    	if (_valid.count(useful_data.second + i) == 0) { // existed char
    		if (idx + i < _data.size()) {
    			_data.insert(_data.begin() + idx + i, make_pair(useful_data.first[i], i+useful_data.second)); 			
    		} else {
    			_data.push_back(make_pair(useful_data.first[i], i+useful_data.second));	
    		}
    		_valid.insert(useful_data.second + i);
    	}
    }

    size_t len = 0;
    while (_data.size()!=0 && lookfor == _data.front().second && _output.remaining_capacity() != 0) {

    }

 	// store data in str and write into stream
    while (_data.size()!=0 && lookfor == _data.front().second && _output.remaining_capacity() != 0) {
    	// considering stream's capacity	
		string s(1,_data.front().first);
		_output.write(s);
		_data.pop_front();
		lookfor++;
		// considering eof
    	if (lookfor == endindex) {
    		_output.end_input();
    		return;
    	}
    }

    



    // while (this->_valid[lookfor]) {
    // 	str += this->_data[lookfor];
    // 	this->_unassembled_bytes--;
    // 	lookfor++; else {
    // 		if (endindex == (index + data.length())) endindex 
    // 	}
    // 	if (lookfor == endindex) {
    // 		_output.end_input();
    // 		lookfor = _capacity + 100;
    // 		break;
    //  	} 
    // }
    // if (_output.remaining_capacity() >= str.length()) {
    // 	_output.write(str);	
    // } else {
    // 	_output.write(str.substr(0,_output.remaining_capacity()));	
    // }
    

   
    // for (auto& c = this->_data.begin(), v = this->_valid.begin(); c != this->_data.end() && *v ; ++c, ++v)
    // {
    //     str += *c;
    // }
 
    // _output.write(str);
}

size_t StreamReassembler::unassembled_bytes() const {
	return _data.size();
}

bool StreamReassembler::empty() const {
	return _data.size() == 0;
}

```

## lab2

出师不利，不能正确编译，报错如下

> --   NOTE: You can choose a build type by calling cmake with one of:
> --     -DCMAKE_BUILD_TYPE=Release   -- full optimizations
> --     -DCMAKE_BUILD_TYPE=Debug     -- better debugging experience in gdb
> --     -DCMAKE_BUILD_TYPE=RelASan   -- full optimizations plus address and undefined-behavior sanitizers
> --     -DCMAKE_BUILD_TYPE=DebugASan -- debug plus sanitizers
> CMake Error: The following variables are used in this project, but they are set to NOTFOUND.
> Please set them or make sure they are set and tested correctly in the CMake files:
> LIBPCAP
>     linked by target "tcp_parser" in directory /home/ycl/Desktop/cs1444/sponge/tests
>     linked by target "tcp_parser" in directory /home/ycl/Desktop/cs1444/sponge/tests
>
> -- Configuring incomplete, errors occurred!
> See also "/home/ycl/Desktop/cs1444/sponge/build/CMakeFiles/CMakeOutput.log".
> Makefile:550: recipe for target 'cmake_check_build_system' failed
> make: *** [cmake_check_build_system] Error 1

解决办法是装一个叫 `libpcap-dev`的包

### 疑惑

1. 接收到的segment的ack，isn和receiver的ack，isn相同吗
2. 如果不能用reassembler的私有成员lookfor，怎么去设置和更新ackno？又和reassembler那样类似？不会太多余了吗？
3. segment的data存储在哪里啊？buffer？doff？
4. higher 边界怎么确定？传过来的segment的data还能是不连续的？那不是又要搞一个数据结构去存储判断连续？  实际上有别的计算方法
5. payload 似乎没有data？   实际上有
6. buffer.copy() 似乎不是data？   实际上就是
7. isn 需要push到ByteStream中吗？   实际上不需要， isn只是一个seqno，不装有data

### 待修复的bug

1. push data的时候的index有问题， 需要考虑该seg是否有syn标志
2. 发来的segs， 后一个的seqno不一定等于考虑前一个seg后的ack，也就是说两个（甚至多个）seg并不连续，所以需要考虑不同情况来更新ack
3. checkpoint的初始化，目前是初始化为isn， 导致了overflow的bug 

### 已修复bug

1. push data的时候的index有问题， 需要考虑该seg是否有syn标志  （添加if考虑多种情况
2. 发来的segs， 后一个的seqno不一定等于 考虑前一个seg后 的ack，也就是说两个（甚至多个）seg并不连续，所以需要考虑不同情况来更新ack    （通过用ByteStream的bytes_written()来更新ack，同时考虑syn和fin对ack的影响  个人觉得不太好想，且比较巧妙，有几种special case
   1. syn到了
   2. syn和fin一起到
   3. fin先到但是并未reassemble，也即他和前面不连续，不能当下就把fin的1加到ack上，所以用了一个finFlag解决这个问题
3. 更新checkpoint 用 length_in_sequence_space()

### 收获

1. push data的index对应的是stream index，而不是 absolute index
2. stream Index 从0开始， 对应到lab1中的index





## lab3

### 疑惑

1. tick方法是要自己实现？用最大寿命，每次转发-1吗？       不是，tick会自己被调用，参数就是经过的时间
2. timer是基于tick实现的吗？   time 是 retransmission timer
3. 怎么判断是否为consecutive retransmission？？？           就是一次重传到下一次重传之间没有接收到有效的seg
4. 怎么组装一个seg            直接建立一个
5. 可以用queue装outstanding segments 吗？    可以的
6. send_empty_segment，这个seg是不用保存在outstanding segments 中的，但是不是说每个要发出去的seg都是放在outstanding中吗？   不对有一个 segments_out ，送出去的seg放在这里面， outstanding 是我们自己构建的DS
7. 怎么生成一个segment？为啥TCPSegment连个构造函数都没有。。。难道要我先构造header和buffer？？？？？    实际上应该是构建TCPSegment的时候自动初始化了header和buffer
8. 用哪种类型来记录当下的ackno，如果是uint64那传来的ackno是wrappingint32，我们需要对他unwrap，这里的checkpoint是啥。。。？？？难道又要跟receiver一样自己创建一个然后不断update？？       checkpoint就是上次received的ack
9. 

### 待修复bug

### 已修复bug

1. 给的初始代码并没有对一些private 变量进行初始化，要在构造函数的方法里自己去初始化，这实验手册也不说一下，服了

### 收获

1. 自建的timer class， 初始化变量时要按照定义变量的顺序。。。。我tm惊了

## lab4

### 收获

1. 把实验手册上的内容和FAQ上的放在一起，把API可能涉及到的内容分别整理到一起思考、
2. 从关键词定位可能需要自己手写的辅助函数
3. 也就是说先熟悉工具类，搞清楚有哪些API，分别完成了怎样的功能，然后从实验手册中的关键词定位，把每句话定位到每个api

### 总结

- 我一开始写的时候实验手册细看了一遍，但是作用不大，因为我没有把他和API关联起来，我对整个TCPConnection的结构和实现方式还是空白，只知道他有一个sender和一个receiver，但是这两者到底是怎样的相互作用，不知道。更不知道这个类的每个ＡＰＩ要同时处理服务器和客户端，最后只能默默的抄别人代码再花时间看懂，好心酸啊

下面的代码部分api下面有我加的注释，是从实验手册和FAQ上拷贝过来的

```cpp

class TCPConnection {
  private:
    bool _linger_after_streams_finish{true};
    bool _active{true};
    size_t _time_since_last_segment_received{0};
    bool _need_send_rst{false};
    bool push_segments_out(bool send_syn = false) ;
    /*
    On outgoing segments, you’ll want to set the ackno and the ack flag whenever possible. That is, 	whenever the TCPReceiver’s ackno() method returns a std::optional<WrappingInt32> that has a 		value, which you can test with has value
    */
    void unclean_shutdown(bool send_rst) ;
    /*
    [Unclean shutdown] Any segment with the RST flag is sent or received.
    the TCPConnection either sends or receives a segment with the rst flag set. In this case, the 		outbound and inbound ByteStreams should both be in the error state, and active() can return false 	  immediately.也就是说如果主动进行unclean_shutdown，参数为true，过程中要发一个带RST的seg给对面，而收到这	个带RST的seg的被动unclean_shutdown的一方则不必要设置参数为true
    */
    bool clean_shutdown() ;
	/*
	[Clean shutdown] The sender is totally done (meaning, the sender's input stream is at EOF with 		no bytes in flight) AND the receiver is totally done (meaning, the receiver's output stream has 	ended) AND either
		_linger_after_streams_finish is false (no need to linger), or
		time_since_last_segment_received() >= 10 * _cfg.rt_timeout (we've lingered long enough)
		
	If the inbound stream ends before the TCPConnection has reached EOF on its outbound stream, this 	 _linger_after_streams_finish needs to be set to false.
	*/
    bool in_syn_sent() ;
    bool in_syn_recv() ;
  
  public:
    //! \name "Input" interface for the writer
    //!@{

    //! \brief Initiate a connection by sending a SYN segment
    void connect();

    //! \brief Write data to the outbound byte stream, and send it over TCP if possible
    //! \returns the number of bytes from `data` that were actually written.
    size_t write(const std::string &data);

    //! \returns the number of `bytes` that can be written right now.
    size_t remaining_outbound_capacity() const;

    //! \brief Shut down the outbound byte stream (still allows reading incoming data)
    void end_input_stream();
    //!@}

    //! \name "Output" interface for the reader
    //!@{

    //! \brief The inbound byte stream received from the peer
    ByteStream &inbound_stream() { return _receiver.stream_out(); }
    //!@}

    //! \name Accessors used for testing

    //!@{
    //! \brief number of bytes sent and not yet acknowledged, counting SYN/FIN each as one byte
    size_t bytes_in_flight() const;
    //! \brief number of bytes not yet reassembled
    size_t unassembled_bytes() const;
    //! \brief Number of milliseconds since the last segment was received
    size_t time_since_last_segment_received() const;
    //!< \brief summarize the state of the sender, receiver, and the connection
    TCPState state() const { return {_sender, _receiver, active(), _linger_after_streams_finish}; };
    //!@}

    //! \name Methods for the owner or operating system to call
    //!@{

    //! Called when a new segment has been received from the network
    void segment_received(const TCPSegment &seg);
    /*
    If the incoming segment occupies any sequence numbers (length_in_sequence_space() > 0)
	If the TCPReceiver thinks the segment is unacceptable (TCPReceiver::segment_received() returns 		false)
	If the TCPSender thinks the ackno is invalid (TCPSender::ack_received() returns false)
	
	On incoming segments, you’ll want to look at the ackno only if the ack field is set. If so, give   	 that ackno (and window size) to the TCPSender.
    */

    //! Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);

    //! \brief TCPSegments that the TCPConnection has enqueued for transmission.
    //! \note The owner or operating system will dequeue these and
    //! put each one into the payload of a lower-layer datagram (usually Internet datagrams (IP),
    //! but could also be user datagrams (UDP) or any other kind).
    std::queue<TCPSegment> &segments_out() { return _segments_out; }

    //! \brief Is the connection still alive in any way?
    //! \returns `true` if either stream is still running or if the TCPConnection is lingering
    //! after both streams have finished (e.g. to ACK retransmissions from the peer)
    bool active() const;
    //!@}

    //! Construct a new connection from a configuration
    explicit TCPConnection(const TCPConfig &cfg) : _cfg{cfg} {}

    //! \name construction and destruction
    //! moving is allowed; copying is disallowed; default construction not possible

    //!@{
    ~TCPConnection();  //!< destructor sends a RST if the connection is still open
    /*
    Send a Seg with RST If the TCPConnection destructor is called while the connection is still 		active (active() returns true)
    */
    TCPConnection() = delete;
    TCPConnection(TCPConnection &&other) = default;
    TCPConnection &operator=(TCPConnection &&other) = default;
    TCPConnection(const TCPConnection &other) = delete;
    TCPConnection &operator=(const TCPConnection &other) = delete;
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_FACTORED_HH	
```

