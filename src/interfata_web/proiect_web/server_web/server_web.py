import socket
import os
import gzip
import threading

pico_ip = None  


def asculta_broadcast_udp():
    global pico_ip
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    udp.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    udp.bind(('', 5679))
    print("Ascult broadcast UDP de la Pico pe portul 5679...")

    while True:
        try:
            data, addr = udp.recvfrom(1024)
            mesaj = data.decode('utf-8', errors='ignore').strip()
            if "PICO_HERE" in mesaj:
                # salvez IP ul real al placutei
                pico_ip = f"http://{addr[0]}" 
                print(f"\n[UDP]: Pico detectat la adresa: {pico_ip}")
                udp.sendto(b"SERVER_HERE", addr)
        except Exception as e:
            print(f"Eroare UDP: {e}")


threading.Thread(target=asculta_broadcast_udp, daemon=True).start()



def rulare_client(clientsocket, address):
    global pico_ip
    print(f'S-a conectat un client: {address}')

    try:
        cerere = ''
        linieDeStart = ''
        while True:
            data = clientsocket.recv(1024)
            cerere = cerere + data.decode()
            print('S-a citit mesajul: \n---------------------------\n' + cerere + '\n---------------------------')
            pozitie = cerere.find('\r\n')
            if (pozitie > -1):
                linieDeStart = cerere[0:pozitie]
                print('S-a citit linia de start din cerere: ##### ' + linieDeStart + '#####')
            break
        print('S-a terminat cititrea.')

        elemente = linieDeStart.split()
        numeResursa = " "
        if len(elemente) > 1:
            numeResursa = elemente[1]
            if numeResursa == "/":
                numeResursa = "/index.html"

        # browserul cere IP-ul Pico 
        if numeResursa == "/pico-ip":
            ip_de_trimis = pico_ip if pico_ip else "necunoscut"
            print(f"!!! TRIMIT CATRE BROWSER IP-UL: {ip_de_trimis}")
            corp = ip_de_trimis.encode('utf-8')
            raspuns = "HTTP/1.1 200 OK\r\n"
            raspuns += "Content-Type: text/plain\r\n"
            raspuns += "Access-Control-Allow-Origin: *\r\n"
            raspuns += f"Content-Length: {len(corp)}\r\n\r\n"
            clientsocket.sendall(raspuns.encode('utf-8') + corp)
            clientsocket.close()
            return
        # 

        import os
        base_dir = os.path.dirname(os.path.abspath(__file__))
        cale_catre_fisier = os.path.join(base_dir, '..', 'continut' + numeResursa.replace('/', os.sep))

        if os.path.isfile(cale_catre_fisier):
            status_line = "HTTP/1.1 200 OK\r\n"
            with open(cale_catre_fisier, "rb") as f:
                corp_raspuns = f.read()
        else:
            status_line = "HTTP/1.1 404 Not Found\r\n"
            corp_raspuns = ("<html><body><h1>404 Not Found</h1><p>Resursa " + numeResursa + " nu exista.</p></body></html>").encode('utf-8')

        corp_compresat = gzip.compress(corp_raspuns)

        extensie = numeResursa.split('.')[-1].lower()
        tipuri = {
            'html': 'text/html', 'css': 'text/css',
            'js': 'application/js', 'png': 'image/png',
            'jpg': 'image/jpeg', 'jpeg': 'image/jpeg',
            'gif': 'image/gif', 'ico': 'image/x-icon',
            'xml': 'application/xml'
        }
        content_type = tipuri.get(extensie, "text/plain")

        raspuns = status_line
        raspuns += "Content-Length: " + str(len(corp_compresat)) + "\r\n"
        raspuns += "Content-Type: " + content_type + "\r\n"
        raspuns += "Content-Encoding: gzip\r\n"
        raspuns += "Server: server_alina\r\n"
        raspuns += "\r\n"

        print("--- RASPUNSUL TRIMIS CATRE BROWSER ---")
        print(raspuns)
        print("--------------------------------------")

        clientsocket.sendall(raspuns.encode('utf-8') + corp_compresat)

    except Exception as e:
        print(f"Eroare la procesarea clientului: {e}")
    finally:
        clientsocket.close()
        print(f'S-a terminat comunicarea cu clientul {address}.')


serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
serversocket.bind(('', 5678))
serversocket.listen(5)
while True:
    print('#########################################################################')
    print('Serverul asculta potentiali clienti.')
    (clientsocket, address) = serversocket.accept()
    client_thread = threading.Thread(target=rulare_client, args=(clientsocket, address))
    client_thread.start()