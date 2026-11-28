all: producer consumer

producer: producer.cpp
	g++ -o producer producer.cpp -pthread -lrt

consumer: consumer.cpp
	g++ -o consumer consumer.cpp -pthread -lrt

clean:
	rm -f producer consumer /dev/shm/sem.mutex /dev/shm/sem.empty /dev/shm/sem.full /dev/shm/sem.users /dev/shm/shm
