#
# Dockerfile template for Tuxedo 12.2.2
# 
# Download the following files to an empty directory:
#   tuxedo122200_64_Linux_01_x86.zip http://www.oracle.com/technetwork/middleware/tuxedo/downloads/index.html
#

# Pull base image
FROM oracle/serverjre:8

MAINTAINER Judy Liu <judy.liu@oracle.com>

# Common environment variables required for this build (do NOT change)
# --------------------------------------------------------------------
ENV ORACLE_HOME=/u01/oracle \
    JAVA_HOME=/usr/java/default \
    PATH=/usr/java/default/bin:$PATH \
    TUX_PKG=tuxedo122200_64_Linux_01_x86.zip

# Core install doesn't include unzip or gcc, add them
# Setup filesystem and oracle user
# Adjust file permissions, go to /u01 as user 'oracle' to proceed with WLS installation
# ------------------------------------------------------------
RUN yum -y install unzip gcc file hostname which util-linux bc compat-libstdc++-33.x86_64 gcc-c++.x86_64 make.x86_64 && \
    rm -rf /var/cache/yum && \
    mkdir -p /u01 && chmod a+xr /u01 && \
    groupadd -g 1000 oracle && useradd -b /u01 -m -g oracle -u 1000 -s /bin/bash oracle 

# Copy packages
# -------------
COPY tuxedo12.2.2.rsp $TUX_PKG init.sh oraInst.loc /u01/ 
RUN  mv /u01/init.sh /u01/oracle/init.sh && \
     mv /u01/oraInst.loc /etc/ && \
     chown oracle:oracle -R /u01 && \
     chmod +x /u01/oracle/init.sh && \
     mkdir -p /nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4 && \
     chown oracle:oracle -R /nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4
USER oracle

# Install Tuxedo
# ------------------------------------------------------------
RUN cd /u01 && \
      mkdir oraInventory && \
      jar xf $TUX_PKG && \
      cd /u01/Disk1/install && \
      chmod -R +x * && \
      ./runInstaller.sh -responseFile /u01/tuxedo12.2.2.rsp -silent -waitforcompletion && \
      rm -rf /u01/Disk1 \
             /u01/tuxedo12.2.2.rsp \
             /u01/$TUX_PKG

ENV ORACLE_HOME=/u01/oracle/tuxHome
ENV TUXDIR=/u01/oracle/tuxHome/tuxedo12.2.2.0.0
ENV PATH=$PATH:$TUXDIR/bin \
    LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$TUXDIR/lib

#
# Configure network ports
# tlisten	nlsaddr:5001  jmx:5002
# SALT 		http:5010
# WSL		5020
#EXPOSE 5001 5002 5010 5020
#USER root
#RUN yum -y install bind-utils

# Add the Tuxedo Resource Module definition file
ADD --chown=oracle:oracle RM /u01/oracle/tuxHome/tuxedo12.2.2.0.0/udataobj/RM

# Copy the Informix client 2.90 installer
COPY informix_2.90_x86_64.tar.gz /tmp
RUN tar xzf /tmp/informix_2.90_x86_64.tar.gz -C /nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4
RUN rm /nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4/etc/odbc.ini
RUN rm /nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4/etc/sqlhosts
ADD --chown=oracle:oracle odbc.ini /nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4/etc/odbc.ini
ADD --chown=oracle:oracle sqlhosts /nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4/etc/sqlhosts

USER oracle
# Set Informix Client environment variables 
ENV PATH=$PATH:/nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4/bin
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4/lib/cli:/nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4/lib/esql:/nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4/lib:/u01/oracle/sourcepro/lib
ENV INFORMIXDIR=/nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4
ENV INFORMIXSERVER="informix"
ENV ONCONFIG="onconfig"
ENV ODBCINI=/nfs/packages/mdx/rhas/x86_64/databases/informix/SDK2.90.FC4/etc/odbc.ini

# Create directory mount points for local drives that will be shared with the container
RUN mkdir -p /u01/oracle/user_projects
RUN mkdir -p /u01/oracle/sourcepro
ADD --chown=oracle:oracle sourcepro/ /u01/oracle/sourcepro
ADD --chown=oracle:oracle unix.mak /u01/oracle/sourcepro/tests/xa/tuxedo/unix.mak
ADD --chown=oracle:oracle testxaclient.cpp /u01/oracle/sourcepro/tests/xa/tuxedo/testxaclient.cpp
ADD --chown=oracle:oracle testxaserver.cpp /u01/oracle/sourcepro/tests/xa/tuxedo/testxaserver.cpp
# RUN mkdir -p /u01/oracle/tuxHome/logs

# Copy the Tuxedo configuration file and logging configuration file into the image
ADD --chown=oracle:oracle ubbconfig /u01/oracle/tuxHome/tuxedo12.2.2.0.0
ADD --chown=oracle:oracle crtlog /u01/oracle/tuxHome/tuxedo12.2.2.0.0

# Set up the environment variables
ENV TUXCONFIG=/u01/oracle/tuxHome/tuxedo12.2.2.0.0/tuxconfig
ENV TLOGDEVICE=/u01/oracle/tuxHome/logs
ENV CC=g++

WORKDIR /u01/oracle/tuxHome/tuxedo12.2.2.0.0
# Define ENTRYPOINT. 
# ENTRYPOINT ["/u01/oracle/init.sh"]