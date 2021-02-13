dummy_allocator.so: dummy_allocator.o
	$(CC) -shared $^ -o $@

dummy_allocator.o: dummy_allocator.c
	$(CC) -c $^ -o $@

clean:
	rm -f dummy_allocator.o dummy_allocator.so

.PHONY: clean
