cec-fix: main.cc
	g++ -I /opt/vc/include -L /opt/vc/lib -l bcm_host -l vchiq_arm -l vcos main.cc -o cec-fix

clean:
	rm cec-fix
