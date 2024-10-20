FROM spcleth/praas-examples:latex-texlive-base as texlive
FROM spcleth/praas:process-python
ENV example latex-editor-python

ENV PATH=/var/task/texlive/2017/bin/x86_64-linux/:$PATH
RUN apt update && apt install -y --no-install-recommends perl

ADD ${example}/ /function
RUN /bin/bash /function/install.sh

ADD configure.py /function
ADD install.py /function
RUN /function/install.py /function \
    && /function/configure.py /function/functions.json.in /function/functions.json /function \
    && rm /function/functions.json.in \
    && rm /function/configure.py \
    && rm /function/install.py \
    && rm /function/install.sh

COPY --from=texlive /var/task/texlive/ /var/task/texlive/
