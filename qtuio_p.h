#ifndef QTUIO_P_H
#define QTUIO_P_H

inline bool qt_readOscString(const QByteArray &source, QByteArray &dest, quint32 &pos)
{
    int end = source.indexOf('\0', pos);
    if (end < 0) {
        pos = source.size();
        dest = QByteArray();
        return false;
    }

    dest = source.mid(pos, end - pos);

    // Skip additional NULL bytes at the end of the string to make sure the
    // total number of bits a multiple of 32 bits ("OSC-string" in the
    // specification).
    end += 4 - ((end - pos) % 4);

    pos = end;
    return true;
}


#endif
