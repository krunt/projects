#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <vector>
#include <pthread.h>
#include <iostream>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct MyPoint {
    float v[3];
    float &operator[](int i) { return v[i]; }
};

struct MyWinding {
    std::vector<MyPoint> p;
    MyPoint &operator[](int i) { return p[i]; }
};

struct MyFrame {
    void Init() {
        w.clear();
    }

    bool Empty() const {
        return w.empty();
    }

    MyPoint b[2];
    std::vector<MyWinding> w;
};

int activeFrame = 0;
MyFrame frame[2];
pthread_t worker;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

void DrawActiveFrame() {
    int i, j, oldFrame;

    pthread_mutex_lock(&mut);
    oldFrame = activeFrame;
    pthread_mutex_unlock(&mut);

    MyFrame &f = frame[oldFrame];

    if (f.Empty())
        return;


	for ( i = 0; i < f.w.size(); i++ ) {
	    glColor3f( 1.0f, 0.0f, 0.0f );

	    glBegin (GL_POLYGON);
        for ( j = 0; j < f.w[i].p.size(); ++j ) {
		    //glVertex3f( 4096+f.w[i][j][0], 4096+f.w[i][j][1], 
                    //(f.w[i][j][2] + 1024) / 2048.0f );
		    glVertex3f( f.w[i][j][0], f.w[i][j][1], f.w[i][j][2] );
                    //(f.w[i][j][2] + 1024) / 2048.0f );
        }
		glVertex3f( f.w[i][0][0], f.w[i][0][1], f.w[i][0][2] );
	    glEnd ();
    }

	for ( i = 0; i < f.w.size(); i++ ) {
	    glColor3f( 0.0f, 1.0f, 1.0f );

	    glBegin (GL_LINE_LOOP);
        for ( j = 0; j < f.w[i].p.size(); ++j ) {
		    //glVertex3f( 4096+f.w[i][j][0], 4096+f.w[i][j][1], 
                    //(f.w[i][j][2] + 1024) / 2048.0f );
		    glVertex3f( f.w[i][j][0], f.w[i][j][1], f.w[i][j][2] );
                    //(f.w[i][j][2] + 1024) / 2048.0f );
        }
		glVertex3f( f.w[i][0][0], f.w[i][0][1], f.w[i][0][2] );
	    glEnd ();
    }

}

void RenderScene(void)
	{
	// Clear the window with current clearing color
	glClear(GL_COLOR_BUFFER_BIT);

    DrawActiveFrame();

        /*
	    glColor3f( 0.3f, 0.0f, 0.0f );

	    glBegin (GL_LINE_LOOP);
		glVertex3f( 4096+1000, 4096+1000, -512.5 );
		glVertex3f( 4096+1000, 4096-1000, 1 );
		glVertex3f( 4096-1000, 4096+1000, 1 );
		glVertex3f( 4096-1000, 4096-1000, 512 );
	    glEnd ();
        */

	    glutSwapBuffers();
	}


void SetupRC()
	{

        glViewport(0, 0, 8192, 8192);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-4096, 4096, -4096, 4096, -4096, 4096);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);


        gluLookAt(512, 1024, -512,
                  0, 0, 0,
                  0, 1, 0 );

	// Gray background
	glClearColor(0.60f, 0.60f, 0.60f, 1.0f );
	}


bool BeginScene(int peer_fd) {
    int i, j, m;
    float s[6], *p = s;

    fprintf(stderr, "BeginScene()\n");

    if ((m=read(peer_fd, s, 24)) != 24)
        return false;

    MyFrame &f = frame[!activeFrame];

    for (i = 0; i < 2; ++i)
        for (j = 0; j < 3; ++j)
            f.b[i][j] = *p++;

    return true;
}

bool EndScene(int peer_fd) {
    int i, j;
    fprintf(stderr, "EndScene()\n");

    pthread_mutex_lock(&mut);
    frame[activeFrame].Init();
    activeFrame = !activeFrame;
    pthread_mutex_unlock(&mut);

    MyFrame &f = frame[activeFrame];

    /*
    fprintf(stderr, "%f/%f %f/%f %f/%f\n", f.b[0][0], f.b[1][0], f.b[0][1], f.b[1][1],
            f.b[0][2], f.b[0][2]);
            */

	for ( i = 0; i < f.w.size(); i++ ) {
        fprintf(stderr, "winding %d", i);
        for ( j = 0; j < f.w[i].p.size(); ++j ) {
		    glVertex3f( f.w[i][j][0], f.w[i][j][1], f.w[i][j][2] );
            fprintf(stderr, " p[%d]=(%f , %f , %f)\n", 
                j, f.w[i][j][0], f.w[i][j][1], f.w[i][j][2] );
        }
        fprintf(stderr, "\n");
    }

    glutPostRedisplay();

    return true;
}

bool ReadWindings(int peer_fd) {
    int m, i, j, num;
    float ptemp[3];

    fprintf(stderr, "ReadWindings()\n");

    if ((m=read(peer_fd, &num, 4)) != 4)
        return false;

    MyFrame &f = frame[!activeFrame];
    MyPoint p;
    MyWinding w;
    for (i = 0; i < num; ++i) {
        if ((m=read(peer_fd, ptemp, 12)) != 12)
            return false;
        p[0] = -ptemp[2];
        p[1] = -ptemp[0];
        p[2] = ptemp[1];
        w.p.push_back(p);
    }
    f.w.push_back(w);

    return true;
}

void NetworkServe(int peer_fd) {
    int m;
    unsigned char cmd;

    while (1) {
        if ((m=read(peer_fd, &cmd, 1)) != 1)
            goto failure;

        switch (cmd) {
        case 0xFF:
            m = BeginScene(peer_fd);
            break;
        case 0xFE:
            m = EndScene(peer_fd);
            break;
        case 0xA0:
            m = ReadWindings(peer_fd);
            break;
        };

        if (!m) {
            exit(2);
        }
    }

failure:
    //disconnect
    if (!m)
        return;

    exit(2);
}

void *NetworkThread(void *arg) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        exit(1);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(25001);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&one, sizeof(one)) == -1) {
        exit(1);
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        exit(1);
    }

    if (listen(fd, 127) == -1) {
        exit(1);
    }

    while (1) {
        int peer_fd = accept(fd, NULL, NULL);
        if (peer_fd < 0) {
            exit(2);
        }

        NetworkServe(peer_fd);
    }

    return NULL;
}

int main(int argc, char* argv[])
	{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(800,600);
	glutCreateWindow("dmap visualizer");
	glutDisplayFunc(RenderScene);
	SetupRC();

    pthread_create(&worker, NULL, &NetworkThread, NULL);

	glutMainLoop();

	return 0;
	}

