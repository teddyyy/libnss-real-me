libnss-real-me
==============

The libnss-real-me name service switch module resolves the name
"realme.localhost" to the IP address for connecting to the Internet.

install
--------------

```
$ make  
$ sudo make install
```

setup
--------------

```
$ sudo sed -i -re 's/^(hosts: .*files)(.*)$/\1 real_me\2/' /etc/nsswitch.conf
```
