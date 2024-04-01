# Do not modify this file.

import os
import sys
import json
import zlib
import base64
import hashlib
import textwrap
import traceback
import threading
import webbrowser
import tkinter as tk
from io import BytesIO
from queue import Queue
from tkinter.ttk import Frame, Label, Entry, Button
from tkinter import Tk, simpledialog, messagebox, PhotoImage, TOP, BOTH, X, LEFT, RIGHT, END, NORMAL

try:
    import websocket # must install websocket-client
    from PIL import Image # must install Pillow
    import requests # must install requests
except:
    raise Exception("You must pip3 install websocket-client requests Pillow")

VERSION = 6

coin = "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"

class AuthDialog(Frame):
    def __init__(self):
        super().__init__()
        self.outputs = []
        self.initUI()

    def initUI(self):
        self.master.title("Welcome to the COMP 310 Virtual World!")
        self.pack(fill=BOTH, expand=True)

        frame0 = Frame(self)
        frame0.pack(fill=X)
        img0 = PhotoImage(data=coin)
        imageLabel = Label(frame0, image=img0)
        imageLabel.image = img0
        imageLabel.pack(side=TOP, pady=16)

        #lbl0.pack(side=LEFT)
        #self.entry0 = Entry(frame0)
        #self.entry0.pack(fill=X)

        frame1 = Frame(self)
        frame1.pack(fill=X)

        lbl1 = Label(frame1, text="Username (public)", width=15)
        lbl1.pack(side=LEFT, padx=25, pady=10)

        self.entry1 = Entry(frame1)
        self.entry1.pack(fill=X, padx=25, expand=True)

        frame2 = Frame(self)
        frame2.pack(fill=X)

        lbl2 = Label(frame2, text="McGill email (private)", width=15)
        lbl2.pack(side=LEFT, padx=25, pady=10)

        self.entry2 = Entry(frame2)
        self.entry2.pack(fill=X, padx=25, expand=True)

        frame3 = Frame(self)
        frame3.pack(fill=X)

        # Command tells the form what to do when the button is clicked
        btn = Button(frame3, text="Register for a new account", command=self.onSubmit)
        btn.pack(padx=5, pady=10)

        frame4 = Frame(self)
        frame4.pack(fill=X)
        credits = "The COMP 310 Virtual World is based on work done by Jonathan Campbell and many past TAs, CAs, and TEAM Mentors in COMP 202, including "
        names = ["Alex B.", "Charlotte S.", "Dasha B.", "Emma T.", "Hamza J.", "Jenny Y.", "Kadence C.", "Ken S.", "Lucas C.", "Morgan C.", "Naomie L.", "Nikola K.", "Victor C.", "Vivian L.", "Michael H.", "Zachary D.", "Alexa I."]
        names.sort()
        credits += ', '.join(names)
        credits += "\nCoin image by Angela H.\nSpecial thanks to DELTARUNE by Toby Fox."
        lbl4 = Label(frame4, text=credits, wraplength=350, anchor="center", justify="center")
        lbl4.pack(fill=X, expand=True)


    def onSubmit(self, *args, **kwargs):
        self.outputs = [self.entry1.get(), self.entry2.get()]
        self.quit()

def send(data_dict, ws=None):
    data_dict["VERSION"] = str(VERSION)
    data_s = json.dumps(data_dict)

    if ws is None:
        ws = websocket.WebSocket()
        ws.connect("wss://infinite-fortress-70189.herokuapp.com/submit")
    ws.send(data_s)
    if ws is None:
        ws.close()

def send_and_receive(data_dict):
    data_dict["VERSION"] = str(VERSION)
    data_s = json.dumps(data_dict)

    ws = websocket.WebSocket()
    ws.connect("wss://infinite-fortress-70189.herokuapp.com/submit")
    
    ws.send(data_s)
    response = ws.recv()
    
    ws.close()
    
    response = json.loads(response)
    
    return response

def insert_message(messages, message):
    lines = message.split('\n')

    short_lines = []
    for line in lines:
        if len(line) > 100:
            short_lines.extend(textwrap.fill(line, 100).split('\n'))
        else:
            short_lines.append(line)
    for line in short_lines:
        messages.insert(tk.END, line)

    messages.yview(tk.END)

q = Queue()
def rcv_thread(email, ticket, password, messages, q):
    def on_open(ws):
        send({
            "type": "register",
            "email": email,
            "password": password,
            "hash": ticket,
        }, ws)

    def on_message(ws, message):
        #message = message.decode('utf-8')
        try:
            data = json.loads(message)
        except:
            print("Couldn't decode:")
            print(message)
            return

        if 'drive' in data:
            disk_data = data['drive']
            disk_data = base64.b64decode(disk_data)
            disk_data = zlib.decompress(disk_data)
            with open(f"{data['drive_name']}", "wb") as f:
                f.write(disk_data)
            del data['drive']

        print(data)

        if 'description' in data:
            insert_message(messages, data['room_name'])
            insert_message(messages, data['description'])
        elif 'image_url' in data:
            response = requests.get(data['image_url'])
            img = Image.open(BytesIO(response.content))
            img.show()
        elif 'room_name' in data:
            insert_message(messages, f"[{data['room_name']}] {data['handle']}: {data['text']}")
        elif 'url' in data:
            webbrowser.open(data['url'], new=0, autoraise=True)
            insert_message(messages, f"{data['handle']}: {data['text']}")
        else:
            insert_message(messages, f"{data['handle']}: {data['text']}")


    def on_error(ws, error):
        print(traceback.format_exc())
        insert_message(messages, f"*** ERROR ***: {error}")

    def on_close(ws, close_status_code, close_msg):
        insert_message(messages, f"*** SERVER ***: Connection closed by server: {close_msg} ({close_status_code})")

    ws_rcv = websocket.WebSocketApp("wss://infinite-fortress-70189.herokuapp.com/receive",
                              on_open=on_open,
                              on_message=on_message,
                              on_error=on_error,
                              on_close=on_close)
    ws_rcv.run_forever(ping_interval=3, skip_utf8_validation=True)

def send_from_input_field(messages, email, ticket, text_input):
    text = text_input.get()
    text_input.delete(0, tk.END)
    if not q.empty():
        messages.insert(tk.END, f"*** CLIENT ***: You were disconnected and cannot send further messages.")
        return
    send({
        'email': email,
        'hash': ticket,
        'text': text,
    })

def authenticate():
    window = tk.Tk()    
    window.title('infinite-fortress-70189')
    
    if not os.path.exists('auth.json'):
        window_height = 500
        window_width = 440

        screen_width = window.winfo_screenwidth()
        screen_height = window.winfo_screenheight()

        x = int((screen_width/2) - (window_width/2))
        y = int((screen_height/2) - (window_height/2))

        window.geometry(f"{window_height}x{window_width}+{x}+{y}")
        dialog = AuthDialog()
        window.bind('<Return>', dialog.onSubmit)
        window.mainloop()
        handle, email = dialog.outputs
        
        response = send_and_receive({
            'type': 'register',
            'handle': handle,
            'email': email,
        })
        assert 'text' in response, response
        if response['text'].startswith('Error'):
            messagebox.showerror("Error", response['text'][7:])
            return {}, ""
        else:
            messagebox.showinfo("Account created", response['text'])
        
        password = simpledialog.askstring("Enter password", "Please enter your password (sent to you by email)", parent=window, show="*")
        password = str(hashlib.sha256(bytes(password, 'utf-8')).digest())

        response = send_and_receive({
            'type': 'register',
            'email': email,
            'password': password
        })
        
        if 'text' in response and response['text'].startswith('Error'):
            messagebox.showerror("Error", response['text'][7:])
            window.destroy()
            return {}, ""
        
        ticket = response['text']
        
        auth_dict = {'email': email, 'password': password}
        json.dump(auth_dict, open('auth.json', 'w'))

        messagebox.showerror("Success", "Authentication successful. Please restart client.py.")
        window.destroy()
        sys.exit()
    else:
        auth_dict = json.load(open('auth.json'))
        email, password = auth_dict['email'], auth_dict['password']
        assert all(x in auth_dict for x in ['email', 'password'])
        response = send_and_receive({
            'type': 'register',
            'email': email,
            'password': password
        })
        
        if response['text'].startswith('Error'):
            messagebox.showerror("Error", response['text'][7:])
            window.destroy()
            return {}, ""
        
        ticket = response['text']
    
    window.destroy()
    return auth_dict, ticket

def start_gui():
    auth_dict, ticket = authenticate()
    if len(auth_dict) == 0:
        return False
    email, password = auth_dict['email'], auth_dict['password']

    window = tk.Tk()
    window.title('infinite-fortress-70189')

    window_height = 700
    window_width = 580

    screen_width = window.winfo_screenwidth()
    screen_height = window.winfo_screenheight()

    x = int((screen_width/2) - (window_width/2))
    y = int((screen_height/2) - (window_height/2))
    window.geometry(f"{window_height}x{window_width}+{x}+{y}")

    frm_messages = tk.Frame(master=window)
    scrollbar = tk.Scrollbar(master=frm_messages)
    messages = tk.Listbox(
        master=frm_messages,
        yscrollcommand=scrollbar.set
    )
    scrollbar.pack(side=tk.RIGHT, fill=tk.Y, expand=False)
    messages.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    frm_messages.grid(row=0, column=0, columnspan=2, padx=10, pady=10, sticky="nsew")

    frm_entry = tk.Frame(master=window)
    text_input = tk.Entry(master=frm_entry)
    text_input.pack(fill=tk.BOTH, expand=True)
    text_input.bind("<Return>", lambda x: send_from_input_field(messages, email, ticket, text_input))
    text_input.insert(0, "Type your message here. Then press return to send.")
    # TODO: On click, default message clears itself.

    def delete_field(event):
       text_input.configure(state=NORMAL)
       text_input.delete(0, END)
       text_input.unbind('<Button-1>', clicked)
    clicked = text_input.bind('<Button-1>', delete_field)

    #btn_send = tk.Button(
    #    master=window,
    #    text='Send',
    #    command=lambda: send_from_input_field(messages, email, ticket, text_input)
    #)

    frm_entry.grid(row=1, column=0, columnspan=2, pady=5, padx=20, sticky="ew")
    #btn_send.grid(row=1, column=1, pady=10, sticky="ew")
    window.rowconfigure(0, minsize=500, weight=1)
    window.rowconfigure(1, minsize=50, weight=0)
    window.columnconfigure(0, minsize=500, weight=1)
    window.columnconfigure(1, minsize=200, weight=0)

    window.update_idletasks()
    window.update()

    rcv_t = threading.Thread(target=rcv_thread, args=(email, ticket, password, messages, q))
    rcv_t.start()

    window.mainloop()

if __name__ == '__main__':
    start_gui()
