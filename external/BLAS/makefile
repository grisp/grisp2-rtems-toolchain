download: blas.tgz cblas.tgz
	tar -xzvf blas.tgz
	tar -xzvf cblas.tgz

blas.tgz:
	wget https://netlib.org/blas/blas.tgz
cblas.tgz:
	wget https://netlib.org/blas/blast-forum/cblas.tgz

.PHONY: clean
clean:
	rm -f *.tgz
	rm -rf BLAS-3.11.0
	rm -rf CBLAS
	
