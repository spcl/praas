FROM public.ecr.aws/lambda/python:3.10

#RUN yum install -y latexmk texlive-base texlive-latex-extra texlive-latex-recommended aspell cm-super texlive-plain-generic texlive-fonts-recommended

ADD texlive.profile .
RUN yum install -y zip curl perl perl-Digest-MD5 wget tar gzip
RUN wget http://mirror.ctan.org/systems/texlive/tlnet/install-tl-unx.tar.gz \
    && tar -xf install-tl-unx.tar.gz && cd install-tl-* \
    && ./install-tl --profile ../texlive.profile
ENV PATH=/var/task/texlive/2017/bin/x86_64-linux/:$PATH
RUN tlmgr install latexmk xkeyval xstring microtype etoolbox booktabs totpages environ textcase hyperxmp ifmtarg xcolor ncctools cmap caption float comment preprint cm-super

# Copy requirements.txt
COPY requirements.txt ${LAMBDA_TASK_ROOT}

# Install the specified packages
RUN pip install -r requirements.txt

# Copy function code
COPY files.py ${LAMBDA_TASK_ROOT}

# Copy function code
COPY compile.py ${LAMBDA_TASK_ROOT}

# Set the CMD to your handler (could also be done as a parameter override outside of the Dockerfile)
CMD [ "files.update_file" ]
