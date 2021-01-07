.PHONY: clean

parrots: http.c main.c rio.c utils.c tpool.c
	gcc $^ -g -o $@ -pthread -lm

clean:
	rm ./proxy
