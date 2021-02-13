dummy_allocator.so: dummy_allocator.o
	$(CC) -shared $^ -o $@

dummy_allocator.o: dummy_allocator.c
	$(CC) -DDUMMY_ALLOCATOR_LOG -fPIC -c $^ -o $@

clean:
	rm -f dummy_allocator.o dummy_allocator.so

.PHONY: clean
