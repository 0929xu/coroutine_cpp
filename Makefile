# 编译器
CXX = g++

# 编译器标志
CXXFLAGS = -Wall -Wextra -std=c++17 -g

# 链接器标志
LDFLAGS = -lpthread

# 目标文件
TARGET = coroutine_test

# 源文件
SRCS = coroutine_cpp.cpp main.cpp

# 依赖文件（对象文件）
OBJS = $(SRCS:.cpp=.o)

# 生成目标
all: $(TARGET)

# 链接步骤
$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

# 编译步骤
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理生成文件
clean:
	rm -f $(OBJS) $(TARGET)

# 伪目标
.PHONY: all clean
