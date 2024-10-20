FROM spcleth/praas-examples:latex-texlive-base as texlive
FROM public.ecr.aws/lambda/python:3.10

COPY --from=texlive /var/task/texlive/ /var/task/texlive/
RUN yum install -y zip curl perl perl-Digest-MD5 wget tar gzip
ENV PATH=/var/task/texlive/2017/bin/x86_64-linux/:$PATH

# Copy requirements.txt
COPY requirements.txt ${LAMBDA_TASK_ROOT}

# Install the specified packages
RUN pip install -r requirements.txt

# Copy function code
COPY files.py ${LAMBDA_TASK_ROOT}

# Copy function code
COPY compile.py ${LAMBDA_TASK_ROOT}

# Set the CMD to your handler (could also be done as a parameter override outside of the Dockerfile)
CMD [ "compile.compile" ]
